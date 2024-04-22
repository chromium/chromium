// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/earl_grey/earl_grey_test.h"

#import "base/json/json_string_value_serializer.h"
#import "base/strings/sys_string_conversions.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/policy/policy_constants.h"
#import "ios/chrome/browser/policy/model/policy_app_interface.h"
#import "ios/chrome/browser/policy/model/policy_earl_grey_matchers.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/app_launch_manager.h"

using chrome_test_util::ToolsMenuView;
using policy::AssertContextMenuItemDisabled;
using policy::AssertContextMenuItemEnabled;
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

  AssertContextMenuItemEnabled(IDS_IOS_TOOLS_MENU_NEW_TAB);
  AssertContextMenuItemEnabled(IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB);
}

// When the IncognitoModeAvailability policy is set to disabled, the "New
// Incognito Tab" item should be disabled in the popup menu triggered by
// long-pressing the tab grid button.
- (void)testTabGridButtonLongPressMenuWhenIncognitoDisabled {
  SetIncognitoAvailabiliy(IncognitoAvailability::kDisabled);
  // Long press the tab grid button.
  [[EarlGrey selectElementWithMatcher:TabGridButton()]
      performAction:grey_longPress()];

  AssertContextMenuItemEnabled(IDS_IOS_TOOLS_MENU_NEW_TAB);
  AssertContextMenuItemDisabled(IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB);
}

// When the IncognitoModeAvailability policy is set to forced, the "New Tab"
// item should be disabled in the popup menu triggered by long-pressing the tab
// grid button.
- (void)testTabGridButtonLongPressMenuWhenIncognitoOnly {
  SetIncognitoAvailabiliy(IncognitoAvailability::kOnly);
  // Long press the tab grid button.
  [[EarlGrey selectElementWithMatcher:TabGridButton()]
      performAction:grey_longPress()];

  AssertContextMenuItemDisabled(IDS_IOS_TOOLS_MENU_NEW_TAB);
  AssertContextMenuItemEnabled(IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB);
}

// Tests that when the IncognitoModeAvailability policy is set to forced, the
// "New Tab" keyboard shortcut action is disabled and can't open a new regular
// tab. This doesn't verify the tab grid UI.
- (void)testOpenNewTab_FromPhysicalKeyboard_ForcedIncognito {
  // Restart the app with the incognito policy.
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  // Configure the policy to force sign-in.
  config.additional_args.push_back(
      "-" + base::SysNSStringToUTF8(kPolicyLoaderIOSConfigurationKey));
  config.additional_args.push_back(
      "<dict><key>IncognitoModeAvailability</key><integer>2</integer></dict>");
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // Use the `CMD + n` keyboard shorcut to try opening a regular tab.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"n"
                                          flags:UIKeyModifierCommand];

  // Verify that the browser view is still in incognito mode.
  GREYAssertTrue([ChromeEarlGrey isIncognitoMode],
                 @"should stay in incognito mode");
}

// Tests that when the IncognitoModeAvailability policy is set to disabled, the
// "New Incognito Tab" keyboard shortcut action is disabled and can't open a new
// incognito tab. This doesn't verify the tab grid UI.
- (void)testOpenNewTab_FromPhysicalKeyboard__DisabledIncognito {
  // Restart the app to take into consideration the policy value.
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  // Configure the policy to force sign-in.
  config.additional_args.push_back(
      "-" + base::SysNSStringToUTF8(kPolicyLoaderIOSConfigurationKey));
  config.additional_args.push_back(
      "<dict><key>IncognitoModeAvailability</key><integer>1</integer></dict>");
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // Use the `CMD + SHIFT + n` keyboard shorcut to try opening an incognito tab.
  [ChromeEarlGrey
      simulatePhysicalKeyboardEvent:@"n"
                              flags:UIKeyModifierCommand | UIKeyModifierShift];

  GREYAssertFalse([ChromeEarlGrey isIncognitoMode],
                  @"should stay in regular mode");
}

// TODO(crbug.com/40163908): Add test to new tab long-press menu.

@end
