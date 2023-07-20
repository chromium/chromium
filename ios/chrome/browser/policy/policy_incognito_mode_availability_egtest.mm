// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/earl_grey/earl_grey_test.h"

#import "base/json/json_string_value_serializer.h"
#import "base/strings/sys_string_conversions.h"
#import "components/policy/policy_constants.h"
#import "ios/chrome/browser/policy/policy_app_interface.h"
#import "ios/chrome/browser/policy/policy_earl_grey_matchers.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ToolsMenuView;
using policy::AssertButtonInCollectionDisabled;
using policy::AssertButtonInCollectionEnabled;
using policy::AssertOverflowMenuElementDisabled;
using policy::AssertOverflowMenuElementEnabled;

namespace {

// Values of the incognito mode availability.
enum class IncognitoAvailability {
  kAvailable = 0,
  kDisabled = 1,
  kOnly = 2,
  kMaxValue = kOnly,
};

// Sets the incognito mode availability.
void SetIncognitoAvailabiliy(IncognitoAvailability availability) {
  [PolicyAppInterface
      setPolicyValue:[NSString stringWithFormat:@"%d",
                                                static_cast<int>(availability),
                                                nil]
              forKey:base::SysUTF8ToNSString(
                         policy::key::kIncognitoModeAvailability)];
}

// Returns a matcher for the tab grid button.
id<GREYMatcher> TabGridButton() {
  return chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_IOS_TOOLBAR_SHOW_TABS);
}

}  // namespace

// Test case to verify that the IncognitoModeAvailability policy is set and
// respected.
@interface PolicyIncognitoModeAvailabilityTestCase : ChromeTestCase
@end

@implementation PolicyIncognitoModeAvailabilityTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  // Use commandline args to insert fake policy data into NSUserDefaults. To the
  // app, this policy data will appear under the
  // "com.apple.configuration.managed" key.
  AppLaunchConfiguration config;
  config.relaunch_policy = NoForceRelaunchAndResetState;
  return config;
}

- (void)tearDown {
  [super tearDown];
}

// When the IncognitoModeAvailability policy is set to available, the tools
// menu item "New Tab" and "New Incognito Tab" should be enabled.
- (void)testToolsMenuWhenIncognitoAvailable {
  SetIncognitoAvailabiliy(IncognitoAvailability::kAvailable);
  [ChromeEarlGreyUI openToolsMenu];

  AssertOverflowMenuElementEnabled(kToolsMenuNewTabId);
  AssertOverflowMenuElementEnabled(kToolsMenuNewIncognitoTabId);
}

// When the IncognitoModeAvailability policy is set to disabled, the tools menu
// item "New Incognito Tab" should be disabled.
- (void)testToolsMenuWhenIncognitoDisabled {
  SetIncognitoAvailabiliy(IncognitoAvailability::kDisabled);
  [ChromeEarlGreyUI openToolsMenu];

  AssertOverflowMenuElementEnabled(kToolsMenuNewTabId);
  AssertOverflowMenuElementDisabled(kToolsMenuNewIncognitoTabId);
}

// When the IncognitoModeAvailability policy is set to forced, the tools menu
// item "New Tab" should be disabled.
- (void)testToolsMenuWhenIncognitoOnly {
  SetIncognitoAvailabiliy(IncognitoAvailability::kOnly);
  [ChromeEarlGreyUI openToolsMenu];

  AssertOverflowMenuElementDisabled(kToolsMenuNewTabId);
  AssertOverflowMenuElementEnabled(kToolsMenuNewIncognitoTabId);
}

// When the IncognitoModeAvailability policy is set to available, the "New Tab"
// and "New Incognito Tab" items should be enabled in the popup menu triggered
// by long-pressing the tab grid button.
- (void)testTabGridButtonLongPressMenuWhenIncognitoAvailable {
  SetIncognitoAvailabiliy(IncognitoAvailability::kAvailable);
  // Long press the tab grid button.
  [[EarlGrey selectElementWithMatcher:TabGridButton()]
      performAction:grey_longPress()];

  AssertButtonInCollectionEnabled(IDS_IOS_TOOLS_MENU_NEW_TAB);
  AssertButtonInCollectionEnabled(IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB);
}

// When the IncognitoModeAvailability policy is set to disabled, the "New
// Incognito Tab" item should be disabled in the popup menu triggered by
// long-pressing the tab grid button.
- (void)testTabGridButtonLongPressMenuWhenIncognitoDisabled {
  SetIncognitoAvailabiliy(IncognitoAvailability::kDisabled);
  // Long press the tab grid button.
  [[EarlGrey selectElementWithMatcher:TabGridButton()]
      performAction:grey_longPress()];

  AssertButtonInCollectionEnabled(IDS_IOS_TOOLS_MENU_NEW_TAB);
  AssertButtonInCollectionDisabled(IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB);
}

// When the IncognitoModeAvailability policy is set to forced, the "New Tab"
// item should be disabled in the popup menu triggered by long-pressing the tab
// grid button.
- (void)testTabGridButtonLongPressMenuWhenIncognitoOnly {
  SetIncognitoAvailabiliy(IncognitoAvailability::kOnly);
  // Long press the tab grid button.
  [[EarlGrey selectElementWithMatcher:TabGridButton()]
      performAction:grey_longPress()];

  AssertButtonInCollectionDisabled(IDS_IOS_TOOLS_MENU_NEW_TAB);
  AssertButtonInCollectionEnabled(IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB);
}

// TODO(crbug.com/1165655): Add test to new tab long-press menu.

@end
