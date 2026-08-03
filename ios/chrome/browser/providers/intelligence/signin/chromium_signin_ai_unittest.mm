// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "ios/public/provider/chrome/browser/intelligence/signin/signin_ai_logo.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using ChromiumAITest = PlatformTest;

// Tests that GetAITierName returns the expected strings in open source.
TEST_F(ChromiumAITest, TestGetAITierName) {
  EXPECT_NSEQ(nil, ios::provider::GetAITierName(-1));
  EXPECT_NSEQ(nil, ios::provider::GetAITierName(0));
  EXPECT_NSEQ(@"1", ios::provider::GetAITierName(1));
  EXPECT_NSEQ(@"2", ios::provider::GetAITierName(2));
  EXPECT_NSEQ(@"3", ios::provider::GetAITierName(3));
  EXPECT_NSEQ(@"4", ios::provider::GetAITierName(4));
}

// Tests that GetAITierFullName returns the expected strings in open source.
TEST_F(ChromiumAITest, TestGetAITierFullName) {
  EXPECT_NSEQ(nil, ios::provider::GetAITierFullName(-1));
  EXPECT_NSEQ(nil, ios::provider::GetAITierFullName(0));
  EXPECT_NSEQ(@"AI 1", ios::provider::GetAITierFullName(1));
  EXPECT_NSEQ(@"AI 2", ios::provider::GetAITierFullName(2));
  EXPECT_NSEQ(@"AI 3", ios::provider::GetAITierFullName(3));
  EXPECT_NSEQ(@"AI 4", ios::provider::GetAITierFullName(4));
}

// Tests that GetPremiumDiscImage returns a valid image.
TEST_F(ChromiumAITest, TestGetPremiumDiscImage) {
  UIImage* image = ios::provider::GetPremiumDiscImage();
  EXPECT_NSNE(nil, image);
}
