// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/json/json_string_value_serializer.h"
#import "base/strings/sys_string_conversions.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/policy/policy_constants.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/policy/model/policy_app_interface.h"
#import "ios/chrome/browser/policy/model/policy_earl_grey_matchers.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_metrics.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

using chrome_test_util::ContainsPartialText;
using chrome_test_util::TabGridIncognitoTabsPanelButton;
using chrome_test_util::TabGridNewIncognitoTabButton;
using chrome_test_util::TabGridOpenTabsPanelButton;
using chrome_test_util::ToolsMenuView;
using policy::AssertContextMenuItemDisabled;
using policy::AssertContextMenuItemEnabled;
using policy::AssertOverflowMenuElementDisabled;
using policy::AssertOverflowMenuElementEnabled;

namespace {

// Message shown in the disabled regular tab grid.
NSString* const kDisabledRegularTabGridMessage =
    @"Your organization requires you to browse privately.\nLearn more";

// Message shown in the disabled incognito tab grid.
NSString* const kDisabledIncognitoTabGridMessage =
    @"Your organization turned off private browsing.";

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

@property BOOL histogramTesterCreated;

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

- (void)setupAndRegisterHistogramTester {
  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Failed to set up histogram tester.");
  self.histogramTesterCreated = YES;
}

- (void)tearDownHelper {
  if (self.histogramTesterCreated) {
    GREYAssertNil([MetricsAppInterface releaseHistogramTester],
                  @"Failed to release histogram tester.");
    self.histogramTesterCreated = NO;
  }
  [super tearDownHelper];
}

// Restarts the app with the given incognito policy.
- (void)restartWithIncognitoPolicy:(IncognitoAvailability)availability {
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  // Configure the policy to force sign-in.
  config.additional_args.push_back(
      "-" + base::SysNSStringToUTF8(kPolicyLoaderIOSConfigurationKey));
  std::string incognito_availability_arg = base::SysNSStringToUTF8(
      [NSString stringWithFormat:@"<dict><key>IncognitoModeAvailability</"
                                 @"key><integer>%d</integer></dict>",
                                 static_cast<int>(availability)]);
  config.additional_args.push_back(incognito_availability_arg);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
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

// Tests the incognito tab grid when the IncognitoModeAvailability policy is set
// to available.
- (void)testIncognitoTabGridWhenIncognitoAvailable {
  // The tab grid is only updated on restart.
  [self restartWithIncognitoPolicy:IncognitoAvailability::kAvailable];
  [self setupAndRegisterHistogramTester];

  // Restart the app with the incognito policy.
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;

  // Open the tab switcher.
  [ChromeEarlGrey showTabSwitcher];

  // Messages from the disabled regular tab grid should not be displayed.
  [[EarlGrey selectElementWithMatcher:ContainsPartialText(
                                          kDisabledRegularTabGridMessage)]
      assertWithMatcher:grey_nil()];

  // Open incognito tab grid.
  [[EarlGrey selectElementWithMatcher:TabGridIncognitoTabsPanelButton()]
      performAction:grey_tap()];

  GREYAssertNil(
      [MetricsAppInterface
          expectUniqueSampleWithCount:1
                            forBucket:static_cast<int>(
                                          IncognitoGridStatus::
                                              kEnabledByEnterprisePolicies)
                         forHistogram:@(kUMAIncognitoGridStatusHistogram)],
      @"Should record incognito grid status metrics");

  // New Incognito Tab button `(+)` should be enabled.
  [[EarlGrey selectElementWithMatcher:TabGridNewIncognitoTabButton()]
      assertWithMatcher:grey_enabled()];

  // Messages from the disabled incognito tab grid should not be displayed.
  [[EarlGrey selectElementWithMatcher:ContainsPartialText(
                                          kDisabledIncognitoTabGridMessage)]
      assertWithMatcher:grey_nil()];

  GREYAssertNil([MetricsAppInterface
                    expectTotalCount:1
                        forHistogram:@(kUMAIncognitoGridStatusHistogram)],
                @"Incognito grid metrics have incorrect total count.");
}

// Tests the incognito tab grid when the IncognitoModeAvailability policy is set
// to disabled.
- (void)testIncognitoTabGridWhenIncognitoDisabled {
  // The tab grid is only updated on restart.
  [self restartWithIncognitoPolicy:IncognitoAvailability::kDisabled];
  [self setupAndRegisterHistogramTester];

  // Open incognito tab grid.
  [ChromeEarlGrey showTabSwitcher];
  [[EarlGrey selectElementWithMatcher:TabGridIncognitoTabsPanelButton()]
      performAction:grey_tap()];

  GREYAssertNil(
      [MetricsAppInterface
          expectUniqueSampleWithCount:1
                            forBucket:static_cast<int>(
                                          IncognitoGridStatus::
                                              kDisabledByEnterprisePolicies)
                         forHistogram:@(kUMAIncognitoGridStatusHistogram)],
      @"Should record incognito grid status metrics");

  // New Incognito Tab button `(+)` should be disabled.
  [[EarlGrey selectElementWithMatcher:TabGridNewIncognitoTabButton()]
      assertWithMatcher:grey_not(grey_enabled())];

  // The disabled incognito tab grid should display a message.
  [[EarlGrey selectElementWithMatcher:ContainsPartialText(
                                          kDisabledIncognitoTabGridMessage)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check that the edit button is disabled.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridEditButton()]
      assertWithMatcher:grey_not(grey_enabled())];

  GREYAssertNil([MetricsAppInterface
                    expectTotalCount:1
                        forHistogram:@(kUMAIncognitoGridStatusHistogram)],
                @"Incognito grid metrics have incorrect total count.");
}

// Tests the incognito tab grid when the IncognitoModeAvailability policy is set
// to forced.
- (void)testIncognitoTabGridWhenIncognitoOnly {
  // The tab grid is only updated on restart.
  [self restartWithIncognitoPolicy:IncognitoAvailability::kOnly];
  [self setupAndRegisterHistogramTester];

  // Open the tab switcher. The incognito tab grid is displayed by default.
  [ChromeEarlGrey showTabSwitcher];

  GREYAssertNil(
      [MetricsAppInterface
          expectUniqueSampleWithCount:1
                            forBucket:static_cast<int>(
                                          IncognitoGridStatus::
                                              kEnabledByEnterprisePolicies)
                         forHistogram:@(kUMAIncognitoGridStatusHistogram)],
      @"Should record incognito grid status metrics");

  // New Incognito Tab button `(+)` should be enabled.
  [[EarlGrey selectElementWithMatcher:TabGridNewIncognitoTabButton()]
      assertWithMatcher:grey_enabled()];

  // Messages from the disabled incognito tab grid should not be displayed.
  [[EarlGrey selectElementWithMatcher:ContainsPartialText(
                                          kDisabledIncognitoTabGridMessage)]
      assertWithMatcher:grey_nil()];

  // Open the regular tab grid.
  [[EarlGrey selectElementWithMatcher:TabGridOpenTabsPanelButton()]
      performAction:grey_tap()];

  // The disabled regular tab grid should display a message.
  [[EarlGrey selectElementWithMatcher:ContainsPartialText(
                                          kDisabledRegularTabGridMessage)]
      assertWithMatcher:grey_sufficientlyVisible()];

  GREYAssertNil([MetricsAppInterface
                    expectTotalCount:1
                        forHistogram:@(kUMAIncognitoGridStatusHistogram)],
                @"Incognito grid metrics have incorrect total count.");
}

// Tests that when the IncognitoModeAvailability policy is set to forced, the
// "New Tab" keyboard shortcut action is disabled and can't open a new regular
// tab. This doesn't verify the tab grid UI.
- (void)testOpenNewTab_FromPhysicalKeyboard_ForcedIncognito {
  [self restartWithIncognitoPolicy:IncognitoAvailability::kOnly];

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
  [self restartWithIncognitoPolicy:IncognitoAvailability::kDisabled];

  // Use the `CMD + SHIFT + n` keyboard shorcut to try opening an incognito tab.
  [ChromeEarlGrey
      simulatePhysicalKeyboardEvent:@"n"
                              flags:UIKeyModifierCommand | UIKeyModifierShift];

  GREYAssertFalse([ChromeEarlGrey isIncognitoMode],
                  @"should stay in regular mode");
}

// TODO(crbug.com/40163908): Add test to new tab long-press menu.

@end
