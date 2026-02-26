// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/metrics/user_action_tester.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "testing/platform_test.h"

namespace {
const char kCameraFlowOSCameraAuthorizationRequestGranted[] =
    "MobileGeminiCameraFlowOSCameraAuthorizationRequestGranted";
const char kCameraFlowOSCameraAuthorizationRequestDenied[] =
    "MobileGeminiCameraFlowOSCameraAuthorizationRequestDenied";
const char kCameraFlowGeminiCameraPermissionAlertAllow[] =
    "MobileGeminiCameraFlowGeminiCameraPermissionAlertAllow";
const char kCameraFlowGeminiCameraPermissionAlertDontAllow[] =
    "MobileGeminiCameraFlowGeminiCameraPermissionAlertDontAllow";
const char kCameraFlowGoToOSSettingsAlertGoToSettings[] =
    "MobileGeminiCameraFlowGoToOSSettingsAlertGoToSettings";
const char kCameraFlowGoToOSSettingsAlertNoThanks[] =
    "MobileGeminiCameraFlowGoToOSSettingsAlertNoThanks";
const char kCameraFlowBegan[] = "MobileGeminiCameraFlowBegan";
const char kCameraFlowPresentCameraPicker[] =
    "MobileGeminiCameraFlowCameraPickerPresented";
const char kCameraFlowCameraPickerCancelled[] =
    "MobileGeminiCameraFlowCameraPickerCancelled";
const char kCameraFlowCameraPickerFinishedWithoutImage[] =
    "MobileGeminiCameraFlowCameraPickerFinishedWithoutImage";
const char kCameraFlowCameraPickerFinishedWithImage[] =
    "MobileGeminiCameraFlowCameraPickerFinishedWithImage";
const char kFeedbackThumbsUp[] = "MobileGeminiFeedbackThumbsUp";
const char kFeedbackThumbsDown[] = "MobileGeminiFeedbackThumbsDown";
const char kImageActionButtonTapped[] = "MobileGeminiImageActionButtonTapped";
const char kInputPlateAttachmentOptionTapped[] =
    "MobileGeminiInputPlateAttachmentOptionTapped";
}  // namespace

class GeminiMetricsTest : public PlatformTest {
 protected:
  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;
};

// Tests that the context menu entry point tapped metric is recorded correctly
// for different aspect ratios.
TEST_F(GeminiMetricsTest, TestImageRemixContextMenuEntryPointMetrics) {
  // Very tall (< 0.3).
  RecordImageRemixContextMenuEntryPointTapped(0.2);
  histogram_tester_.ExpectBucketCount(
      kImageRemixContextMenuEntryPointAspectRatioTappedHistogram,
      IOSGeminiAspectRatioBucket::kVeryTall, 1);

  // Tall (< 0.8).
  RecordImageRemixContextMenuEntryPointTapped(0.5);
  histogram_tester_.ExpectBucketCount(
      kImageRemixContextMenuEntryPointAspectRatioTappedHistogram,
      IOSGeminiAspectRatioBucket::kTall, 1);

  // Slightly tall (< 1.0).
  RecordImageRemixContextMenuEntryPointTapped(0.9);
  histogram_tester_.ExpectBucketCount(
      kImageRemixContextMenuEntryPointAspectRatioTappedHistogram,
      IOSGeminiAspectRatioBucket::kSlightlyTall, 1);

  // Perfect square (== 1.0).
  RecordImageRemixContextMenuEntryPointTapped(1.0);
  histogram_tester_.ExpectBucketCount(
      kImageRemixContextMenuEntryPointAspectRatioTappedHistogram,
      IOSGeminiAspectRatioBucket::kPerfectSquare, 1);

  // Slightly wide (<= 1.2).
  RecordImageRemixContextMenuEntryPointTapped(1.1);
  histogram_tester_.ExpectBucketCount(
      kImageRemixContextMenuEntryPointAspectRatioTappedHistogram,
      IOSGeminiAspectRatioBucket::kSlightlyWide, 1);

  // Wide (<= 1.7).
  RecordImageRemixContextMenuEntryPointTapped(1.5);
  histogram_tester_.ExpectBucketCount(
      kImageRemixContextMenuEntryPointAspectRatioTappedHistogram,
      IOSGeminiAspectRatioBucket::kWide, 1);

  // Very wide (> 1.7).
  RecordImageRemixContextMenuEntryPointTapped(2.0);
  histogram_tester_.ExpectBucketCount(
      kImageRemixContextMenuEntryPointAspectRatioTappedHistogram,
      IOSGeminiAspectRatioBucket::kVeryWide, 1);

  // Unknown (<= 0).
  RecordImageRemixContextMenuEntryPointTapped(0.0);
  histogram_tester_.ExpectBucketCount(
      kImageRemixContextMenuEntryPointAspectRatioTappedHistogram,
      IOSGeminiAspectRatioBucket::kUnknown, 1);
  RecordImageRemixContextMenuEntryPointTapped(-1.0);
  histogram_tester_.ExpectBucketCount(
      kImageRemixContextMenuEntryPointAspectRatioTappedHistogram,
      IOSGeminiAspectRatioBucket::kUnknown, 2);
}

TEST_F(GeminiMetricsTest, RecordGeminiCameraFlowOSAuthorizationResult) {
  RecordGeminiCameraFlowOSAuthorizationResult(true);

  histogram_tester_.ExpectUniqueSample(
      kCameraFlowOSAuthorizationRequestResultHistogram,
      IOSGeminiCameraFlowOSCameraAuthorizationResult::kGranted, 1);
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   kCameraFlowOSCameraAuthorizationRequestGranted));
  EXPECT_EQ(0, user_action_tester_.GetActionCount(
                   kCameraFlowOSCameraAuthorizationRequestDenied));

  RecordGeminiCameraFlowOSAuthorizationResult(false);

  histogram_tester_.ExpectBucketCount(
      kCameraFlowOSAuthorizationRequestResultHistogram,
      IOSGeminiCameraFlowOSCameraAuthorizationResult::kDenied, 1);
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   kCameraFlowOSCameraAuthorizationRequestGranted));
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   kCameraFlowOSCameraAuthorizationRequestDenied));
}

TEST_F(GeminiMetricsTest,
       RecordGeminiCameraFlowGeminiCameraPermissionAlertResult) {
  RecordGeminiCameraFlowGeminiCameraPermissionAlertResult(true);

  histogram_tester_.ExpectUniqueSample(
      kCameraFlowGeminiCameraPermissionAlertResultHistogram,
      IOSGeminiCameraPermissionAlertResult::kAllow, 1);
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   kCameraFlowGeminiCameraPermissionAlertAllow));
  EXPECT_EQ(0, user_action_tester_.GetActionCount(
                   kCameraFlowGeminiCameraPermissionAlertDontAllow));

  RecordGeminiCameraFlowGeminiCameraPermissionAlertResult(false);

  histogram_tester_.ExpectBucketCount(
      kCameraFlowGeminiCameraPermissionAlertResultHistogram,
      IOSGeminiCameraPermissionAlertResult::kDontAllow, 1);
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   kCameraFlowGeminiCameraPermissionAlertAllow));
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   kCameraFlowGeminiCameraPermissionAlertDontAllow));
}

TEST_F(GeminiMetricsTest, RecordGeminiCameraFlowGoToOSSettingsAlertResult) {
  RecordGeminiCameraFlowGoToOSSettingsAlertResult(true);

  histogram_tester_.ExpectUniqueSample(
      kCameraFlowGoToOSSettingsAlertResultHistogram,
      IOSGeminiGoToOSSettingsAlertResult::kGoToSettings, 1);
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   kCameraFlowGoToOSSettingsAlertGoToSettings));
  EXPECT_EQ(0, user_action_tester_.GetActionCount(
                   kCameraFlowGoToOSSettingsAlertNoThanks));

  RecordGeminiCameraFlowGoToOSSettingsAlertResult(false);

  histogram_tester_.ExpectBucketCount(
      kCameraFlowGoToOSSettingsAlertResultHistogram,
      IOSGeminiGoToOSSettingsAlertResult::kNoThanks, 1);
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   kCameraFlowGoToOSSettingsAlertGoToSettings));
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   kCameraFlowGoToOSSettingsAlertNoThanks));
}

// Tests that response latency is recorded to the correct histograms based on
// page context and generated image presence.
TEST_F(GeminiMetricsTest, TestResponseLatencyMetrics) {
  base::TimeDelta latency = base::Milliseconds(100);

  // Case 1: Page context present & generated image present.
  RecordResponseLatency(latency, true, true);
  histogram_tester_.ExpectTimeBucketCount(kResponseLatencyWithContextHistogram,
                                          latency, 1);
  histogram_tester_.ExpectTimeBucketCount(
      kResponseLatencyWithGeneratedImageHistogram, latency, 1);
  histogram_tester_.ExpectTotalCount(kResponseLatencyWithoutContextHistogram,
                                     0);
  histogram_tester_.ExpectTotalCount(
      kResponseLatencyWithoutGeneratedImageHistogram, 0);

  // Case 2: Page context present & generated image absent.
  RecordResponseLatency(latency, true, false);
  histogram_tester_.ExpectTimeBucketCount(kResponseLatencyWithContextHistogram,
                                          latency, 2);
  histogram_tester_.ExpectTimeBucketCount(
      kResponseLatencyWithoutGeneratedImageHistogram, latency, 1);
  histogram_tester_.ExpectTotalCount(kResponseLatencyWithoutContextHistogram,
                                     0);
  histogram_tester_.ExpectTimeBucketCount(
      kResponseLatencyWithGeneratedImageHistogram, latency, 1);

  // Case 3: Page context absent, generated image present.
  RecordResponseLatency(latency, false, true);
  histogram_tester_.ExpectTimeBucketCount(kResponseLatencyWithContextHistogram,
                                          latency, 2);
  histogram_tester_.ExpectTimeBucketCount(
      kResponseLatencyWithoutContextHistogram, latency, 1);
  histogram_tester_.ExpectTimeBucketCount(
      kResponseLatencyWithGeneratedImageHistogram, latency, 2);
  histogram_tester_.ExpectTimeBucketCount(
      kResponseLatencyWithoutGeneratedImageHistogram, latency, 1);

  // Case 4: Page context absent, generated image absent.
  RecordResponseLatency(latency, false, false);
  histogram_tester_.ExpectTimeBucketCount(kResponseLatencyWithContextHistogram,
                                          latency, 2);
  histogram_tester_.ExpectTimeBucketCount(
      kResponseLatencyWithoutContextHistogram, latency, 2);
  histogram_tester_.ExpectTimeBucketCount(
      kResponseLatencyWithGeneratedImageHistogram, latency, 2);
  histogram_tester_.ExpectTimeBucketCount(
      kResponseLatencyWithoutGeneratedImageHistogram, latency, 2);
}

TEST_F(GeminiMetricsTest, RecordGeminiCameraFlowBegan) {
  RecordGeminiCameraFlowBegan();
  EXPECT_EQ(1, user_action_tester_.GetActionCount(kCameraFlowBegan));
}

TEST_F(GeminiMetricsTest,
       RecordGeminiCameraFlowOSCameraAuthorizationInitialStatus) {
  RecordGeminiCameraFlowOSCameraAuthorizationInitialStatus(
      IOSGeminiOSCameraAuthorizationInitialStatus::kNotDetermined);
  histogram_tester_.ExpectUniqueSample(
      kCameraFlowOSCameraAuthorizationInitialStatusHistogram,
      IOSGeminiOSCameraAuthorizationInitialStatus::kNotDetermined, 1);
}

TEST_F(GeminiMetricsTest,
       RecordGeminiCameraFlowGeminiCameraPermissionInitialValue) {
  RecordGeminiCameraFlowGeminiCameraPermissionInitialValue(true);
  histogram_tester_.ExpectUniqueSample(
      kCameraFlowGeminiCameraPermissionInitialValueHistogram, true, 1);
}

TEST_F(GeminiMetricsTest, RecordGeminiCameraFlowPresentCameraPicker) {
  RecordGeminiCameraFlowPresentCameraPicker();
  EXPECT_EQ(1,
            user_action_tester_.GetActionCount(kCameraFlowPresentCameraPicker));
}

TEST_F(GeminiMetricsTest, RecordGeminiCameraFlowCameraPickerResult) {
  RecordGeminiCameraFlowCameraPickerResult(
      IOSGeminiCameraPickerResult::kCancelled);
  histogram_tester_.ExpectUniqueSample(kCameraFlowCameraPickerResultHistogram,
                                       IOSGeminiCameraPickerResult::kCancelled,
                                       1);
  EXPECT_EQ(
      1, user_action_tester_.GetActionCount(kCameraFlowCameraPickerCancelled));
  EXPECT_EQ(0, user_action_tester_.GetActionCount(
                   kCameraFlowCameraPickerFinishedWithoutImage));
  EXPECT_EQ(0, user_action_tester_.GetActionCount(
                   kCameraFlowCameraPickerFinishedWithImage));

  RecordGeminiCameraFlowCameraPickerResult(
      IOSGeminiCameraPickerResult::kFinishedWithoutImage);
  histogram_tester_.ExpectBucketCount(
      kCameraFlowCameraPickerResultHistogram,
      IOSGeminiCameraPickerResult::kFinishedWithoutImage, 1);
  EXPECT_EQ(
      1, user_action_tester_.GetActionCount(kCameraFlowCameraPickerCancelled));
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   kCameraFlowCameraPickerFinishedWithoutImage));
  EXPECT_EQ(0, user_action_tester_.GetActionCount(
                   kCameraFlowCameraPickerFinishedWithImage));

  RecordGeminiCameraFlowCameraPickerResult(
      IOSGeminiCameraPickerResult::kFinishedWithImage);
  histogram_tester_.ExpectBucketCount(
      kCameraFlowCameraPickerResultHistogram,
      IOSGeminiCameraPickerResult::kFinishedWithImage, 1);
  EXPECT_EQ(
      1, user_action_tester_.GetActionCount(kCameraFlowCameraPickerCancelled));
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   kCameraFlowCameraPickerFinishedWithoutImage));
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   kCameraFlowCameraPickerFinishedWithImage));
}

TEST_F(GeminiMetricsTest, RecordGeminiFeedback) {
  RecordGeminiFeedback(IOSGeminiFeedback::kThumbsUp);
  histogram_tester_.ExpectBucketCount(kFeedbackHistogram,
                                      IOSGeminiFeedback::kThumbsUp, 1);
  EXPECT_EQ(1, user_action_tester_.GetActionCount(kFeedbackThumbsUp));

  RecordGeminiFeedback(IOSGeminiFeedback::kThumbsDown);
  histogram_tester_.ExpectBucketCount(kFeedbackHistogram,
                                      IOSGeminiFeedback::kThumbsDown, 1);
  EXPECT_EQ(1, user_action_tester_.GetActionCount(kFeedbackThumbsDown));
}

TEST_F(GeminiMetricsTest, RecordGeminiImageActionButtonTapped) {
  RecordGeminiImageActionButtonTapped(gemini::ImageActionButtonType::kCopy);
  histogram_tester_.ExpectBucketCount(kImageActionButtonHistogram,
                                      gemini::ImageActionButtonType::kCopy, 1);
  EXPECT_EQ(1, user_action_tester_.GetActionCount(kImageActionButtonTapped));

  RecordGeminiImageActionButtonTapped(gemini::ImageActionButtonType::kDownload);
  histogram_tester_.ExpectBucketCount(
      kImageActionButtonHistogram, gemini::ImageActionButtonType::kDownload, 1);
  EXPECT_EQ(2, user_action_tester_.GetActionCount(kImageActionButtonTapped));

  RecordGeminiImageActionButtonTapped(gemini::ImageActionButtonType::kShare);
  histogram_tester_.ExpectBucketCount(kImageActionButtonHistogram,
                                      gemini::ImageActionButtonType::kShare, 1);
  EXPECT_EQ(3, user_action_tester_.GetActionCount(kImageActionButtonTapped));

  RecordGeminiImageActionButtonTapped(gemini::ImageActionButtonType::kUnknown);
  histogram_tester_.ExpectBucketCount(
      kImageActionButtonHistogram, gemini::ImageActionButtonType::kUnknown, 1);
  EXPECT_EQ(4, user_action_tester_.GetActionCount(kImageActionButtonTapped));
}

TEST_F(GeminiMetricsTest, RecordGeminiInputPlateAttachmentOptionTapped) {
  RecordGeminiInputPlateAttachmentOptionTapped(
      gemini::InputPlateAttachmentOption::kCamera);
  histogram_tester_.ExpectBucketCount(
      kInputPlateAttachmentOptionHistogram,
      gemini::InputPlateAttachmentOption::kCamera, 1);
  EXPECT_EQ(
      1, user_action_tester_.GetActionCount(kInputPlateAttachmentOptionTapped));

  RecordGeminiInputPlateAttachmentOptionTapped(
      gemini::InputPlateAttachmentOption::kGallery);
  histogram_tester_.ExpectBucketCount(
      kInputPlateAttachmentOptionHistogram,
      gemini::InputPlateAttachmentOption::kGallery, 1);
  EXPECT_EQ(
      2, user_action_tester_.GetActionCount(kInputPlateAttachmentOptionTapped));

  RecordGeminiInputPlateAttachmentOptionTapped(
      gemini::InputPlateAttachmentOption::kCreateImageDeselected);
  histogram_tester_.ExpectBucketCount(
      kInputPlateAttachmentOptionHistogram,
      gemini::InputPlateAttachmentOption::kCreateImageDeselected, 1);
  EXPECT_EQ(
      3, user_action_tester_.GetActionCount(kInputPlateAttachmentOptionTapped));

  RecordGeminiInputPlateAttachmentOptionTapped(
      gemini::InputPlateAttachmentOption::kCreateImageSelected);
  histogram_tester_.ExpectBucketCount(
      kInputPlateAttachmentOptionHistogram,
      gemini::InputPlateAttachmentOption::kCreateImageSelected, 1);
  EXPECT_EQ(
      4, user_action_tester_.GetActionCount(kInputPlateAttachmentOptionTapped));

  RecordGeminiInputPlateAttachmentOptionTapped(
      gemini::InputPlateAttachmentOption::kUnknown);
  histogram_tester_.ExpectBucketCount(
      kInputPlateAttachmentOptionHistogram,
      gemini::InputPlateAttachmentOption::kUnknown, 1);
  EXPECT_EQ(
      5, user_action_tester_.GetActionCount(kInputPlateAttachmentOptionTapped));
}

// Tests that the Gemini ineligibility reasons are recorded correctly.
TEST_F(GeminiMetricsTest, RecordGeminiIneligibilityReasons) {
  const char* histogram = kGeminiIneligibilityReasonHistogram;
  RecordGeminiIneligibilityReasons(gemini::IneligibilityReasons());
  histogram_tester_.ExpectTotalCount(histogram, 0);

  gemini::IneligibilityReasons reasons =
      gemini::IneligibilityReasons()
        .set_workspace(true)
        .set_account_capability(true);

  RecordGeminiIneligibilityReasons(reasons);

  histogram_tester_.ExpectBucketCount(
      histogram, IOSGeminiIneligibilityReason::kWorkspaceRestricted, 1);

  histogram_tester_.ExpectBucketCount(
      histogram, IOSGeminiIneligibilityReason::kInsufficientAccountCapability,
      1);

  histogram_tester_.ExpectTotalCount(histogram, 2);
}
