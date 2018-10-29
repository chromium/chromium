// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#include "base/test/scoped_feature_list.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/browsing_data/browsing_data_features.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsMenuPrivacyButton;

@interface ClearBrowsingDataSettingsTestCase : ChromeTestCase
@end

@implementation ClearBrowsingDataSettingsTestCase {
  base::test::ScopedFeatureList _featureList;
}

- (void)openClearBrowsingDataDialog {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsMenuPrivacyButton()];

  NSString* clearBrowsingDataDialogLabel =
      l10n_util::GetNSString(IDS_IOS_CLEAR_BROWSING_DATA_TITLE);
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(
                                          clearBrowsingDataDialogLabel)]
      performAction:grey_tap()];
}

// Test that opening the clear browsing data dialog does not cause a crash
// with the old UI.
- (void)testOpenClearBrowsingDataDialogOldUI {
  _featureList.InitAndDisableFeature(kNewClearBrowsingDataUI);

  [self openClearBrowsingDataDialog];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Test that opening the clear browsing data dialog does not cause a crash
// with the new UI.
- (void)testOpenClearBrowsingDataDialogNewUI {
  _featureList.InitAndEnableFeature(kNewClearBrowsingDataUI);

  [self openClearBrowsingDataDialog];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

@end
