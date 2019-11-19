// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include "base/test/scoped_feature_list.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ContentSettingsButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsMenuBackButton;

@interface TranslateUITestCase : ChromeTestCase
@end

@implementation TranslateUITestCase

// Opens the translate settings page and verifies that accessibility is set up
// properly.
- (void)testAccessibilityOfTranslateSettings {
  // Disable the Language Settings UI.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({}, {kLanguageSettings});

  // Open translate settings.
  // TODO(crbug.com/606815): This and close settings is mostly shared with block
  // popups settings tests, and others. See if this can move to shared code.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:ContentSettingsButton()];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_TRANSLATE_SETTING)]
      performAction:grey_tap()];

  // Assert title and accessibility.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   @"translate_settings_view_controller")]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];

  // Close settings.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

@end
