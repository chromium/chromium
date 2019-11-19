// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/wk_navigation_action_util.h"

#import <WebKit/WebKit.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

namespace {

// Creates description string for WKNavigationAction given parameters.
NSString* MakeDescriptionString(long navigation_type,
                                long click_type,
                                float pos_x,
                                float pos_y) {
  return
      [NSString stringWithFormat:
                    @"<class: 0x12312af; navigationType = %ld; "
                    @"syntheticClickType = %ld; position x = %.2f y = %.2f "
                    @"request = null; sourceFrame = null; targetFrame = null>",
                    navigation_type, click_type, pos_x, pos_y];
}
}  // namespace

using WKNavigationActionUtilTest = PlatformTest;

// Tests GetNavigationInitiationTypeFromDescription with different correct
// format descriptions and voiceover is on.
TEST_F(WKNavigationActionUtilTest, InitiationTypeFromDescriptionWithVoiceOver) {
  NavigationActionInitiationType initiation_type =
      NavigationActionInitiationType::kUnknownInitiator;
  initiation_type = GetNavigationActionInitiationTypeWithVoiceOverOn(
      MakeDescriptionString(0, 0, 0, 0));
  EXPECT_EQ(NavigationActionInitiationType::kUnknownInitiator, initiation_type);
  initiation_type = GetNavigationActionInitiationTypeWithVoiceOverOn(
      MakeDescriptionString(0, 0, 100.0, 25.0));
  EXPECT_EQ(NavigationActionInitiationType::kUserInitiated, initiation_type);
  initiation_type = GetNavigationActionInitiationTypeWithVoiceOverOn(
      MakeDescriptionString(0, 1, 0, 0));
  EXPECT_EQ(NavigationActionInitiationType::kUnknownInitiator, initiation_type);
  initiation_type = GetNavigationActionInitiationTypeWithVoiceOverOn(
      MakeDescriptionString(2, 2, 20.0, 30.4));
  EXPECT_EQ(NavigationActionInitiationType::kUserInitiated, initiation_type);

  initiation_type = GetNavigationActionInitiationTypeWithVoiceOverOn(
      MakeDescriptionString(1, 0, 0, 30.4));
  EXPECT_EQ(NavigationActionInitiationType::kUserInitiated, initiation_type);
}

// Tests GetNavigationInitiationTypeFromDescription with different correct
// format descriptions and voiceover is off.
TEST_F(WKNavigationActionUtilTest, InitiationTypeFromDescriptionNoVoiceOver) {
  NavigationActionInitiationType initiation_type =
      NavigationActionInitiationType::kUnknownInitiator;
  initiation_type = GetNavigationActionInitiationTypeWithVoiceOverOff(
      MakeDescriptionString(0, 0, 0, 0));
  EXPECT_EQ(NavigationActionInitiationType::kUnknownInitiator, initiation_type);
  initiation_type = GetNavigationActionInitiationTypeWithVoiceOverOff(
      MakeDescriptionString(0, 0, 100.0, 25.0));
  EXPECT_EQ(NavigationActionInitiationType::kUnknownInitiator, initiation_type);
  initiation_type = GetNavigationActionInitiationTypeWithVoiceOverOff(
      MakeDescriptionString(2, 1, 0, 0));
  EXPECT_EQ(NavigationActionInitiationType::kUserInitiated, initiation_type);
  initiation_type = GetNavigationActionInitiationTypeWithVoiceOverOff(
      MakeDescriptionString(0, 2, 20.0, 30.4));
  EXPECT_EQ(NavigationActionInitiationType::kUserInitiated, initiation_type);
}

// Tests GetNavigationInitiationTypeFromDescription given different description
// formats.
TEST_F(WKNavigationActionUtilTest,
       InitiationTypeFromDifferentDescriptionFormats) {
  NavigationActionInitiationType initiation_type =
      NavigationActionInitiationType::kUnknownInitiator;
  NSString* description =
      @"<class: 0x213; position x = 1.2 y = 3.4 request = null; "
      @"syntheticClickType = 1; request = null>";
  initiation_type =
      GetNavigationActionInitiationTypeWithVoiceOverOff(description);
  EXPECT_EQ(NavigationActionInitiationType::kUserInitiated, initiation_type);
  initiation_type =
      GetNavigationActionInitiationTypeWithVoiceOverOn(description);
  EXPECT_EQ(NavigationActionInitiationType::kUserInitiated, initiation_type);

  description = @"<class: 0x123; navigationType = 1; position x = 1.2 y = 3.4>";
  initiation_type =
      GetNavigationActionInitiationTypeWithVoiceOverOff(description);
  EXPECT_EQ(NavigationActionInitiationType::kUnknownInitiator, initiation_type);
  initiation_type =
      GetNavigationActionInitiationTypeWithVoiceOverOn(description);
  EXPECT_EQ(NavigationActionInitiationType::kUserInitiated, initiation_type);

  description =
      @"<class: 0x123; navigationType = 1; syntheticClickType = 1; request = "
      @"null>";
  initiation_type =
      GetNavigationActionInitiationTypeWithVoiceOverOff(description);
  EXPECT_EQ(NavigationActionInitiationType::kUserInitiated, initiation_type);
  initiation_type =
      GetNavigationActionInitiationTypeWithVoiceOverOn(description);
  EXPECT_EQ(NavigationActionInitiationType::kUnknownInitiator, initiation_type);
}
}  // namespace web
