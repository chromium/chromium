// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/android/model_downloader_android.h"

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "services/on_device_model/android/on_device_model_bridge_native_unittest_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace on_device_model {

using DownloadFailureReason = ModelDownloaderAndroid::DownloadFailureReason;
using BaseModelSpec = ModelDownloaderAndroid::BaseModelSpec;

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
  base::test::TestFuture<base::expected<BaseModelSpec, DownloadFailureReason>>
      future;
  auto downloader = std::make_unique<ModelDownloaderAndroid>(kFeature);
  downloader->StartDownload(future.GetCallback());
  EXPECT_EQ(future.Get(),
            base::unexpected(DownloadFailureReason::kApiNotAvailable));
}

TEST_F(ModelDownloaderAndroidTest, DownloadAvailable) {
  java_helper_.SetMockAiCoreFactory();

  base::test::TestFuture<base::expected<BaseModelSpec, DownloadFailureReason>>
      future;
  auto downloader = std::make_unique<ModelDownloaderAndroid>(kFeature);
  downloader->StartDownload(future.GetCallback());
  java_helper_.TriggerDownloaderOnAvailable("test_model", "123");
  auto result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->name, "test_model");
  EXPECT_EQ(result->version, "123");
}

TEST_F(ModelDownloaderAndroidTest, DownloadUnavailable) {
  java_helper_.SetMockAiCoreFactory();

  base::test::TestFuture<base::expected<BaseModelSpec, DownloadFailureReason>>
      future;
  auto downloader = std::make_unique<ModelDownloaderAndroid>(kFeature);
  downloader->StartDownload(future.GetCallback());
  java_helper_.TriggerDownloaderOnUnavailable(
      DownloadFailureReason::kUnknownError);
  EXPECT_EQ(future.Get(),
            base::unexpected(DownloadFailureReason::kUnknownError));
}

TEST_F(ModelDownloaderAndroidTest, NativeDownloaderDeletionIsSafe) {
  java_helper_.SetMockAiCoreFactory();

  auto downloader = std::make_unique<ModelDownloaderAndroid>(kFeature);
  downloader->StartDownload(base::DoNothing());
  // Delete the native session manually and ensure async completion doesn't
  // cause a crash.
  downloader.reset();
  java_helper_.TriggerDownloaderOnAvailable("test_model", "123");
}

}  // namespace
}  // namespace on_device_model
