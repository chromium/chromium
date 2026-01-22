// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"

#import "base/test/metrics/histogram_tester.h"
#import "testing/platform_test.h"

class GeminiMetricsTest : public PlatformTest {
 protected:
  base::HistogramTester histogram_tester_;
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
