// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_font_size_utils.h"

#import "base/test/task_environment.h"
#import "components/dom_distiller/core/distilled_page_prefs.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Test fixture for the reader mode font size helper functions.
class ReaderModeFontSizeUtilsTest : public PlatformTest {
 protected:
  ReaderModeFontSizeUtilsTest() {
    dom_distiller::DistilledPagePrefs::RegisterProfilePrefs(prefs_.registry());
    distilled_page_prefs_ =
        std::make_unique<dom_distiller::DistilledPagePrefs>(&prefs_);
  }

  base::test::TaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  std::unique_ptr<dom_distiller::DistilledPagePrefs> distilled_page_prefs_;
};

// Tests that IncreaseReaderModeFontSize increases the font size.
TEST_F(ReaderModeFontSizeUtilsTest, IncreaseFontSize) {
  distilled_page_prefs_->SetUserPrefFontScaling(1.0);
  IncreaseReaderModeFontSize(distilled_page_prefs_.get());
  EXPECT_GT(distilled_page_prefs_->GetFontScaling(), 1.0);
}

// Tests that DecreaseReaderModeFontSize decreases the font size.
TEST_F(ReaderModeFontSizeUtilsTest, DecreaseFontSize) {
  distilled_page_prefs_->SetUserPrefFontScaling(1.0);
  DecreaseReaderModeFontSize(distilled_page_prefs_.get());
  EXPECT_LT(distilled_page_prefs_->GetFontScaling(), 1.0);
}

// Tests that ResetReaderModeFontSize resets the font size to 1.0.
TEST_F(ReaderModeFontSizeUtilsTest, ResetFontSize) {
  distilled_page_prefs_->SetUserPrefFontScaling(1.5);
  ResetReaderModeFontSize(distilled_page_prefs_.get());
  EXPECT_EQ(distilled_page_prefs_->GetFontScaling(), 1.0);
}

// Tests that CanIncreaseReaderModeFontSize returns false at the max font size
// and true otherwise.
TEST_F(ReaderModeFontSizeUtilsTest, CanIncreaseFontSize) {
  distilled_page_prefs_->SetUserPrefFontScaling(2.0);
  EXPECT_FALSE(CanIncreaseReaderModeFontSize(distilled_page_prefs_.get()));
  distilled_page_prefs_->SetUserPrefFontScaling(1.0);
  EXPECT_TRUE(CanIncreaseReaderModeFontSize(distilled_page_prefs_.get()));
}

// Tests that CanDecreaseReaderModeFontSize returns false at the min font size
// and true otherwise.
TEST_F(ReaderModeFontSizeUtilsTest, CanDecreaseFontSize) {
  distilled_page_prefs_->SetUserPrefFontScaling(0.5);
  EXPECT_FALSE(CanDecreaseReaderModeFontSize(distilled_page_prefs_.get()));
  distilled_page_prefs_->SetUserPrefFontScaling(1.0);
  EXPECT_TRUE(CanDecreaseReaderModeFontSize(distilled_page_prefs_.get()));
}

// Tests that CanResetReaderModeFontSize returns false when the font size is at
// the default and true otherwise.
TEST_F(ReaderModeFontSizeUtilsTest, CanResetFontSize) {
  distilled_page_prefs_->SetUserPrefFontScaling(1.0);
  EXPECT_FALSE(CanResetReaderModeFontSize(distilled_page_prefs_.get()));
  distilled_page_prefs_->SetUserPrefFontScaling(1.5);
  EXPECT_TRUE(CanResetReaderModeFontSize(distilled_page_prefs_.get()));
}
