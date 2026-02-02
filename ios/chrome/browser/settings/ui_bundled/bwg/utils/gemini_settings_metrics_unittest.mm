// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/bwg/utils/gemini_settings_metrics.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/metrics/user_action_tester.h"
#import "testing/platform_test.h"

namespace {
const char kCameraSettingsClose[] = "MobileGeminiCameraSettingsClose";
const char kCameraSettingsBack[] = "MobileGeminiCameraSettingsBack";
const char kCameraSettingsToggledOn[] =
    "MobileGeminiCameraSettingsGeminiCameraPermissionToggledOn";
const char kCameraSettingsToggledOff[] =
    "MobileGeminiCameraSettingsGeminiCameraPermissionToggledOff";
const char kCameraSettingsToggleHistogram[] =
    "IOS.Gemini.Camera.Settings.GeminiCameraPermissionToggled";
}  // namespace

class GeminiSettingsMetricsTest : public PlatformTest {
 protected:
  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;
};

TEST_F(GeminiSettingsMetricsTest, RecordGeminiCameraSettingsClose) {
  RecordGeminiCameraSettingsClose();
  EXPECT_EQ(1, user_action_tester_.GetActionCount(kCameraSettingsClose));
}

TEST_F(GeminiSettingsMetricsTest, RecordGeminiCameraSettingsBack) {
  RecordGeminiCameraSettingsBack();
  EXPECT_EQ(1, user_action_tester_.GetActionCount(kCameraSettingsBack));
}

TEST_F(GeminiSettingsMetricsTest, RecordGeminiCameraSettingsToggled) {
  RecordGeminiCameraSettingsToggled(YES);
  histogram_tester_.ExpectUniqueSample(kCameraSettingsToggleHistogram, true, 1);
  EXPECT_EQ(1, user_action_tester_.GetActionCount(kCameraSettingsToggledOn));
  EXPECT_EQ(0, user_action_tester_.GetActionCount(kCameraSettingsToggledOff));

  RecordGeminiCameraSettingsToggled(NO);
  histogram_tester_.ExpectBucketCount(kCameraSettingsToggleHistogram, false, 1);
  EXPECT_EQ(1, user_action_tester_.GetActionCount(kCameraSettingsToggledOn));
  EXPECT_EQ(1, user_action_tester_.GetActionCount(kCameraSettingsToggledOff));
}
