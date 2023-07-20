//
// Created by tianyi on 6/15/23.
//

#include "DataCollecting.h"

#include "Tracking.h"
#include "LocalMapping.h"
#include "LoopClosing.h"

namespace ORB_SLAM3
{

DataCollecting::DataCollecting(System* pSys, Atlas *pAtlas, const float bMonocular, bool bInertial, const string &_strSeqName):
        mpSystem(pSys), mbMonocular(bMonocular), mbInertial(bInertial)
{
    //mbMonocular = bMonocular;
    //mbInertial = bInertial;
    //mpSystem = pSys;
    mpAtlas = pAtlas;
    msSeqName = _strSeqName;

    mbFinished = false;
    mbImageFeaturesReady = false;
    mbCurrentFrameFeaturesReady = false;
    mnImCounter = 0;
    mbIsNewFrameProcessed = true; // Initialized as it is already processed

    mnLocalBA = 0;

    InitializeCSVLogger();

    cout << "Data Collecting Module Initialized" << endl;

}


void DataCollecting::SetTracker(Tracking *pTracker)
{
    mpTracker = pTracker;
}

void DataCollecting::SetLocalMapper(LocalMapping *pLocalMapper)
{
    mpLocalMapper = pLocalMapper;
}

void DataCollecting::SetLoopCloser(LoopClosing *pLoopCloser)
{
    mpLoopCloser = pLoopCloser;
}

void DataCollecting::Run()
{
    // Initialize the data collector
    cout << "Data Collecting Module Running" << endl;

    usleep(1*1000*1000);

    while(1)
    {
        //TODO
        if(!mbIsNewFrameProcessed)
        {
            auto start = chrono::high_resolution_clock::now();

            // If the new arrived frame haven't been processed
            if(mbImageFeaturesReady)
            {
                CalculateImageFeatures();
            }
            if(mbCurrentFrameFeaturesReady)
            {
                CalculateCurrentFrameFeatures();
            }

            if (mbImageFeaturesReady && mbCurrentFrameFeaturesReady)
            {
                WriteRowCSVLogger();
            }

            {
                unique_lock<mutex> lock(mMutexNewFrameProcessed);
                mbIsNewFrameProcessed = true;
            }

            auto stop = chrono::high_resolution_clock::now();
            mfDuration = chrono::duration_cast<chrono::microseconds>(stop - start).count();
            //cout << "Run() elapsed: " << mfDuration << " microseconds" << endl;

        }
        usleep(0.01*1000*1000);
    }
}

//////////////////////////////////////////////////////////////////////////
// TODO: Define methods to collect features from ORB-SLAM3
//////////////////////////////////////////////////////////////////////////

//void DataCollecting::DefineMethod2CollectFeatures()
//{
//    // TODO
//}

void DataCollecting::CollectImageTimeStamp(const double &timestamp)
{
    // TODO
    unique_lock<mutex> lock(mMutexImageTimeStamp);
    mdTimeStamp = timestamp;
}

void DataCollecting::CollectImageFileName(string &filename)
{
    // TODO
    unique_lock<mutex> lock(mMutexImageFileName);
    msImgFileName = filename;
}

void DataCollecting::CollectImagePixel(cv::Mat &imGrey)
{
    {
        unique_lock<mutex> lock1(mMutexImagePixel);
        //Option1: save the image with the same resolution
        mImGrey = imGrey.clone();

        //Option2: save the image with reduced resolution (reduced by 1/16 = 1/4*1/4)
        //cv::resize(imGrey, mImGrey, cv::Size(), 0.25, 0.25, cv::INTER_NEAREST);
        // Instead, reduce the image resolution later at the calculation step
    }
    {
        unique_lock<mutex> lock2(mMutexImageCounter);
        mnImCounter ++;
    }
    // update the data ready flag
    mbImageFeaturesReady = true;
    {
        unique_lock<mutex> lock(mMutexNewFrameProcessed);
        mbIsNewFrameProcessed = false;
    }

}


void DataCollecting::CollectCurrentFrame(const Frame &frame)
{
    unique_lock<mutex> lock(mMutexCurrentFrame);
    mCurrentFrame = frame;
    mbCurrentFrameFeaturesReady = true;
}

void DataCollecting::CollectCurrentFrameTrackMode(const int &nTrackMode)
{
    // 0 for Track with Motion
    // 1 for Track with Reference Frame
    // 2 for Track with Relocalization
    unique_lock<mutex> lock(mMutexCurrentFrameTrackMode);
    mnTrackMode = nTrackMode;
}

void DataCollecting::CollectCurrentFramePrePOOutlier(const int &nPrePOOutlier)
{
    unique_lock<mutex> lock(mMutexCurrentFramePrePOOutlier);
    mnPrePOOutlier = nPrePOOutlier;
}

void DataCollecting::CollectCurrentFramePrePOKeyMapLoss(const int &nPrePOKeyMapLoss)
{
    unique_lock<mutex> lock(mMutexCurrentFramePrePOKeyMapLoss);
    mnPrePOKeyMapLoss = nPrePOKeyMapLoss;
}

void DataCollecting::CollectCurrentFrameInlier(const int &nInlier)
{
    unique_lock<mutex> lock(mMutexCurrentFrameInlier);
    mnInlier = nInlier;
}

void DataCollecting::CollectCurrentFramePostPOOutlier(const int &nPostPOOutlier)
{
    unique_lock<mutex> lock(mMutexCurrentFramePostPOOutlier);
    mnPostPOOutlier = nPostPOOutlier;
}

void DataCollecting::CollectCurrentFramePostPOKeyMapLoss(const int &nPostPOKeyMapLoss)
{
    unique_lock<mutex> lock(mMutexCurrentFramePostPOKeyMapLoss);
    mnPostPOKeyMapLoss = nPostPOKeyMapLoss;
}

void DataCollecting::CollectCurrentFrameMatchedInlier(const int &nMatchedInlier)
{
    unique_lock<mutex> lock(mMutexCurrentFrameMatchedInlier);
    mnMatchedInlier = nMatchedInlier;
}

void DataCollecting::CollectCurrentFrameMapPointDepth(const Frame &frame)
{
    unique_lock<mutex> lock(mMutexCurrentFrameMapPointDepth);
    vector<float> vMapPointMinDepth;
    for (int i = 0; i < frame.N; i++)
    {
        // If current frame i-th map point is not Null
        if (frame.mvpMapPoints[i])
            vMapPointMinDepth.push_back(frame.mvpMapPoints[i]->GetMinDistanceInvariance());
    }
    // Calculate the mean
    mfMapPointAvgMinDepth = accumulate(vMapPointMinDepth.begin(), vMapPointMinDepth.end(), 0.0) / vMapPointMinDepth.size();
    // Calculate the variance
    float sum_of_squares = 0;
    for (auto num : vMapPointMinDepth) 
    {
        sum_of_squares += pow(num - mfMapPointAvgMinDepth, 2);
    }
    mfMapPointVarMinDepth = sum_of_squares / vMapPointMinDepth.size();
}

void DataCollecting::CollectLocalMappingBANumber(const int num_FixedKF_BA, const int num_OptKF_BA,
                                                 const int num_MPs_BA,  const int num_edges_BA)
{
    unique_lock<mutex> lock(mMutexLocalMappingBANumber);
    // Fixed Keyframes. Keyframes that see Local MapPoints but that are not Local Keyframes
    mnFixedKF_BA = num_FixedKF_BA;
    // Local keyframes
    mnOptKF_BA = num_OptKF_BA;
    mnMPs_BA = num_MPs_BA;
    mnEdges_BA = num_edges_BA;
}

void DataCollecting::CollectLocalMappingBAOptimizer(const float fLocalBAVisualError)
{
    unique_lock<mutex> lock(mMutexLocalMappingBAOptimizer);
    mfLocalBAVisualError = fLocalBAVisualError;
    mnLocalBA ++;
}

void DataCollecting::CollectLoopClosureBAOptimizer(const float fGlobalBAVisualError)
{
    unique_lock<mutex> lock(mMutexLoopClosureBAOptimizer);
    mfGlobalBAVisualError = fGlobalBAVisualError;
    mnGlobalBA ++;
}

//////////////////////////////////////////////////////////////////////////
// END: Methods to collect features from ORB-SLAM3
//////////////////////////////////////////////////////////////////////////




//////////////////////////////////////////////////////////////////////////
// TODO: Methods to process features from ORB-SLAM3
//////////////////////////////////////////////////////////////////////////

double DataCollecting::CalculateImageEntropy(const cv::Mat& image)
{
    // Calculate histogram of pixel intensities
    cv::Mat hist;
    int histSize = 256;  // Number of bins for intensity values
    float range[] = {0, 256};
    const float* histRange = {range};
    cv::calcHist(&image, 1, 0, cv::Mat(), hist, 1, &histSize, &histRange);

    // Normalize histogram
    hist /= image.total();

    // Calculate entropy
    double entropy = 0;
    for (int i = 0; i < histSize; ++i)
    {
        if (hist.at<float>(i) > 0)
        {
            entropy -= hist.at<float>(i) * std::log2(hist.at<float>(i));
        }
    }
    return entropy;
}

void DataCollecting::CalculateImageFeatures()
{
    unique_lock<mutex> lock1(mMutexImagePixel);
    unique_lock<mutex> lock2(mMutexImageFeatures);
    if (mImGrey.empty())
    {
        cout << "Failed to load image." << endl;
    }
    else
    {
        cv::resize(mImGrey, mImGrey, cv::Size(), 0.25, 0.25, cv::INTER_NEAREST);
        cv::Scalar meanValue, stddevValue;
        // calculate the brightness and contrast
        cv::meanStdDev(mImGrey, meanValue, stddevValue);
        mdBrightness = meanValue[0];
        mdContrast = stddevValue[0];
        // calculate the entropy
        mdEntropy = CalculateImageEntropy(mImGrey);
    }

}


void DataCollecting::CalculateCurrentFrameFeatures()
{
    unique_lock<mutex> lock1(mMutexCurrentFrame); 
    if(mbCurrentFrameFeaturesReady)
    {
        unique_lock<mutex> lock2(mMutexCurrentFrameFeatures);
        mnkeypoints = mCurrentFrame.N;
        // current camera pose in world reference
        Sophus::SE3f currentTwc = mCurrentFrame.GetPose().inverse();
        // get current pose
        mQ = currentTwc.unit_quaternion();
        mtwc = currentTwc.translation();
        // calculate relative pose
        Sophus::SE3f currentRelativePose = mTwc.inverse() * currentTwc;
        Eigen::Matrix3f rotationMatrix = currentRelativePose.rotationMatrix();
        mREuler = rotationMatrix.eulerAngles(2, 1, 0); // ZYX convention
        mRtwc = currentRelativePose.translation();
        // update current camera pose
        mTwc = currentTwc;
    }
}



//
//void DataCollecting::CollectFrameFinalKeypointDistribution()
//{
//    // TODO
//}
//


//
//void DataCollecting::CollectLocalBAVisualError()
//{
//    // Collect the bundle adjustment error of visual input in local mapping module
//    // TODO
//}
//
//void DataCollecting::CollectLocalBAInertialError()
//{
//    // Collect the bundle adjustment error of inertial input in local mapping module
//    // TODO
//}
//
//void DataCollecting::CollectPoseAbsolutefPosition()
//{
//    // TODO
//}
//
//void DataCollecting::CollectPoseAbsoluteRotation()
//{
//    // TODO
//}


//////////////////////////////////////////////////////////////////////////
// END: Methods to process features from ORB-SLAM3
//////////////////////////////////////////////////////////////////////////


void DataCollecting::RequestFinish()
{
    // TODO
    mFileLogger.close();
    mbFinished = true;

}

//////////////////////////////////////////////////////////////////////////
// START: Methods to save features in .CSV
//////////////////////////////////////////////////////////////////////////

void DataCollecting::CollectCurrentTime()
{
    // Get the current time
    std::time_t currentTime = std::time(nullptr);

    // Convert the time to a string
    std::tm* now = std::localtime(&currentTime);

    // Format the date and time string
    std::ostringstream oss;
    oss << std::put_time(now, "%y%m%d_%H%M%S");
    msCurrentTime = oss.str();
}

void DataCollecting::InitializeCSVLogger()
{
    CollectCurrentTime();
    msCSVFileName = msSeqName + "_" + msCurrentTime + ".csv";

    // Open the CSV file for writing
    mFileLogger.open(msCSVFileName);

    // Write the first row with column names
    if (!mFileLogger.is_open()) 
    {
        std::cerr << "Unable to open file: " << msCSVFileName << std::endl;
    }
    else
    {
        // Write column names
        for (const auto& columnName : mvsColumnFeatureNames)
        {
            mFileLogger << columnName << ",";
        }
        mFileLogger << endl;
    }
}


void DataCollecting::WriteRowCSVLogger()
{
    if (!mFileLogger.is_open())
    {
        std::cerr << "Unable to open file: " << msCSVFileName << std::endl;
    }
    else
    {
        {
            unique_lock<mutex> lock(mMutexImageCounter);
            mFileLogger << mnImCounter << ",";
        }
        {
            unique_lock<mutex> lock(mMutexImageTimeStamp);
            mFileLogger << fixed << setprecision(6) << 1e9*mdTimeStamp << ",";
        }
        {
            unique_lock<mutex> lock(mMutexImageFileName);
            mFileLogger << msImgFileName;
        }

        if(mbImageFeaturesReady)
        {
            {
                unique_lock<mutex> lock(mMutexImageFeatures);
                mFileLogger << "," << fixed << std::setprecision(6) << mdBrightness << "," << mdContrast << "," << mdEntropy;
            }
        }

        if(mbCurrentFrameFeaturesReady)
        {
            {
                unique_lock<mutex> lock(mMutexCurrentFrameMapPointDepth);
                mFileLogger  << "," << mfMapPointAvgMinDepth << "," << mfMapPointVarMinDepth << ",";
            }
            {
                unique_lock<mutex> lock(mMutexCurrentFrameTrackMode);
                mFileLogger << mnTrackMode << ",";
            }
            {
                unique_lock<mutex> lock(mMutexCurrentFramePrePOOutlier);
                mFileLogger << mnPrePOOutlier << ",";
            }
            {
                unique_lock<mutex> lock(mMutexCurrentFramePrePOKeyMapLoss);
                mFileLogger << mnPrePOKeyMapLoss << ",";
            }
            {
                unique_lock<mutex> lock(mMutexCurrentFrameInlier);
                mFileLogger << mnInlier << ",";
            }
            {
                unique_lock<mutex> lock(mMutexCurrentFramePostPOOutlier);
                mFileLogger << mnPostPOOutlier << ",";
            }
            {
                unique_lock<mutex> lock(mMutexCurrentFramePostPOKeyMapLoss);
                mFileLogger << mnPostPOKeyMapLoss << ",";
            }
            {
                unique_lock<mutex> lock(mMutexCurrentFrameMatchedInlier);
                mFileLogger << mnMatchedInlier << ",";
            }
            {
                unique_lock<mutex> lock(mMutexCurrentFrameFeatures);
                mFileLogger << mnkeypoints << ",";
                mFileLogger << setprecision(9) << mtwc(0) << "," << mtwc(1) << "," << mtwc(2) << ",";
                mFileLogger << mQ.x() << "," << mQ.y() << "," << mQ.z() << "," << mQ.w() << ",";
                mFileLogger << mRtwc(0) << "," << mRtwc(1) << "," << mRtwc(2) << ",";
                mFileLogger << mREuler(0) << "," << mREuler(1) << "," << mREuler(2) << ",";
            }
            {
                unique_lock<mutex> lock(mMutexLocalMappingBANumber);
                mFileLogger << mnFixedKF_BA << "," << mnOptKF_BA << "," << mnMPs_BA << "," << mnEdges_BA << ",";
            }
            {
                unique_lock<mutex> lock(mMutexLocalMappingBAOptimizer);
                mFileLogger << mnLocalBA << "," << mfLocalBAVisualError << ",";
            }
            {
                unique_lock<mutex> lock(mMutexLoopClosureBAOptimizer);
                mFileLogger << mnGlobalBA << "," << mfGlobalBAVisualError << ",";
            }
            {
                mFileLogger << mfDuration;
            }

        }
        
        mFileLogger << endl;
    }
}


// End of the ORBSLAM3 namespace
}


