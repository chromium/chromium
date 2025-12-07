// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/android/model_downloader_android.h"

#include <optional>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "services/on_device_model/android/downloader_params.mojom.h"
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

  mojom::DownloaderParamsPtr MakeDownloaderParams(
      bool require_persistent_mode) {
    auto params = mojom::DownloaderParams::New();
    params->require_persistent_mode = require_persistent_mode;
    return params;
  }

  ~ModelDownloaderAndroidTest() override = default;

 protected:
  void VerifyHistograms(
      base::expected<void, DownloadFailureReason> failure_reason) {
    bool success = failure_reason.has_value();
    histogram_tester_.ExpectUniqueSample(
        "OnDeviceModel.Android.IsModelDownloadSuccessful", success, 1);
    histogram_tester_.ExpectUniqueSample(
        "OnDeviceModel.Android.IsModelDownloadSuccessful.ScamDetection",
        success, 1);
    if (success) {
      histogram_tester_.ExpectTotalCount(
          "OnDeviceModel.Android.ModelDownloadFailureReason", 0);
      histogram_tester_.ExpectTotalCount(
          "OnDeviceModel.Android.ModelDownloadFailureReason.ScamDetection", 0);
    } else {
      histogram_tester_.ExpectUniqueSample(
          "OnDeviceModel.Android.ModelDownloadFailureReason",
          failure_reason.error(), 1);
      histogram_tester_.ExpectUniqueSample(
          "OnDeviceModel.Android.ModelDownloadFailureReason.ScamDetection",
          failure_reason.error(), 1);
    }
  }

  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  OnDeviceModelBridgeNativeUnitTestHelper java_helper_;
};

TEST_F(ModelDownloaderAndroidTest, DefaultDownloader) {
  base::test::TestFuture<base::expected<BaseModelSpec, DownloadFailureReason>>
      future;
  auto downloader = std::make_unique<ModelDownloaderAndroid>(
      kFeature, MakeDownloaderParams(/*require_persistent_mode=*/false));
  downloader->StartDownload(future.GetCallback());
  EXPECT_EQ(future.Get(),
            base::unexpected(DownloadFailureReason::kApiNotAvailable));

  VerifyHistograms(base::unexpected(DownloadFailureReason::kApiNotAvailable));
}

TEST_F(ModelDownloaderAndroidTest, DownloadAvailable) {
  java_helper_.SetMockAiCoreFactory();

  base::test::TestFuture<base::expected<BaseModelSpec, DownloadFailureReason>>
      future;
  auto downloader = std::make_unique<ModelDownloaderAndroid>(
      kFeature, MakeDownloaderParams(/*require_persistent_mode=*/true));
  downloader->StartDownload(future.GetCallback());
  java_helper_.VerifyDownloaderParams(kFeature,
                                      /*require_persistent_mode=*/true);
  java_helper_.TriggerDownloaderOnAvailable("test_model", "123");
  auto result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->name, "test_model");
  EXPECT_EQ(result->version, "123");

  VerifyHistograms(base::ok());
}

TEST_F(ModelDownloaderAndroidTest, DownloadUnavailable) {
  java_helper_.SetMockAiCoreFactory();

  base::test::TestFuture<base::expected<BaseModelSpec, DownloadFailureReason>>
      future;
  auto downloader = std::make_unique<ModelDownloaderAndroid>(
      kFeature, MakeDownloaderParams(/*require_persistent_mode=*/false));
  downloader->StartDownload(future.GetCallback());
  java_helper_.TriggerDownloaderOnUnavailable(
      DownloadFailureReason::kUnknownError);
  EXPECT_EQ(future.Get(),
            base::unexpected(DownloadFailureReason::kUnknownError));

  VerifyHistograms(base::unexpected(DownloadFailureReason::kUnknownError));
}

TEST_F(ModelDownloaderAndroidTest, DownloadAvailableOnDifferentThread) {
  java_helper_.SetMockAiCoreFactory();

  base::test::TestFuture<base::expected<BaseModelSpec, DownloadFailureReason>>
      future;
  auto downloader = std::make_unique<ModelDownloaderAndroid>(
      kFeature, MakeDownloaderParams(/*require_persistent_mode=*/false));
  java_helper_.SetDownloaderCallbackOnDifferentThread();

  downloader->StartDownload(future.GetCallback());
  java_helper_.TriggerDownloaderOnAvailable("test_model", "123");

  auto result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->name, "test_model");
  EXPECT_EQ(result->version, "123");

  VerifyHistograms(base::ok());
}

TEST_F(ModelDownloaderAndroidTest, DownloadUnavailableOnDifferentThread) {
  java_helper_.SetMockAiCoreFactory();

  base::test::TestFuture<base::expected<BaseModelSpec, DownloadFailureReason>>
      future;
  auto downloader = std::make_unique<ModelDownloaderAndroid>(
      kFeature, MakeDownloaderParams(/*require_persistent_mode=*/false));
  java_helper_.SetDownloaderCallbackOnDifferentThread();

  downloader->StartDownload(future.GetCallback());
  java_helper_.TriggerDownloaderOnUnavailable(
      DownloadFailureReason::kUnknownError);

  EXPECT_EQ(future.Get(),
            base::unexpected(DownloadFailureReason::kUnknownError));

  VerifyHistograms(base::unexpected(DownloadFailureReason::kUnknownError));
}

TEST_F(ModelDownloaderAndroidTest, NativeDownloaderDeletionIsSafe) {
  java_helper_.SetMockAiCoreFactory();

  auto downloader = std::make_unique<ModelDownloaderAndroid>(
      kFeature, MakeDownloaderParams(/*require_persistent_mode=*/false));
  downloader->StartDownload(base::DoNothing());
  // Delete the native session manually and ensure async completion doesn't
  // cause a crash.
  downloader.reset();
  java_helper_.TriggerDownloaderOnAvailable("test_model", "123");
}

}  // namespace
}  // namespace on_device_model
