// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/metrics/user_action_tester.h"
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
