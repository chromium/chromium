// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/android/model_downloader_android.h"

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "services/on_device_model/android/on_device_model_bridge_native_unittest_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace on_device_model {
namespace {

constexpr optimization_guide::proto::ModelExecutionFeature kFeature =
    optimization_guide::proto::ModelExecutionFeature::
        MODEL_EXECUTION_FEATURE_SCAM_DETECTION;

class ModelDownloaderAndroidTest : public testing::Test {
 public:
  ModelDownloaderAndroidTest() = default;
  ~ModelDownloaderAndroidTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  OnDeviceModelBridgeNativeUnitTestHelper java_helper_;
};

TEST_F(ModelDownloaderAndroidTest, DefaultDownloader) {
  base::MockCallback<ModelDownloaderAndroid::OnDownloadCompleteCallback>
      on_download_complete_callback;
  EXPECT_CALL(on_download_complete_callback, Run(false)).Times(1);
  auto downloader = std::make_unique<ModelDownloaderAndroid>(kFeature);
  downloader->StartDownload(on_download_complete_callback.Get());
}

TEST_F(ModelDownloaderAndroidTest, DownloadAvailable) {
  java_helper_.SetMockAiCoreFactory();

  base::MockCallback<ModelDownloaderAndroid::OnDownloadCompleteCallback>
      on_download_complete_callback;
  EXPECT_CALL(on_download_complete_callback, Run(true)).Times(1);
  auto downloader = std::make_unique<ModelDownloaderAndroid>(kFeature);
  downloader->StartDownload(on_download_complete_callback.Get());
  java_helper_.TriggerDownloaderOnAvailable();
}

TEST_F(ModelDownloaderAndroidTest, DownloadUnavailable) {
  java_helper_.SetMockAiCoreFactory();

  base::MockCallback<ModelDownloaderAndroid::OnDownloadCompleteCallback>
      on_download_complete_callback;
  EXPECT_CALL(on_download_complete_callback, Run(false)).Times(1);
  auto downloader = std::make_unique<ModelDownloaderAndroid>(kFeature);
  downloader->StartDownload(on_download_complete_callback.Get());
  java_helper_.TriggerDownloaderOnUnavailable();
}

TEST_F(ModelDownloaderAndroidTest, NativeDownloaderDeletionIsSafe) {
  java_helper_.SetMockAiCoreFactory();

  auto downloader = std::make_unique<ModelDownloaderAndroid>(kFeature);
  downloader->StartDownload(base::DoNothing());
  // Delete the native session manually and ensure async completion doesn't
  // cause a crash.
  downloader.reset();
  java_helper_.TriggerDownloaderOnAvailable();
}

}  // namespace
}  // namespace on_device_model
