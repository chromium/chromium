// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/earl_grey/earl_grey_test.h"

#import "base/json/json_string_value_serializer.h"
#import "base/strings/sys_string_conversions.h"
#import "components/policy/policy_constants.h"
#import "ios/chrome/browser/policy/policy_app_interface.h"
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

// Tests the enabled state of an item.
// `string_id` is the ID of the string associated with the item.
// `enabled` is the expected availability.
void AssertItemEnabled(int string_id, bool enabled) {
  id<GREYMatcher> assertion_matcher =
      enabled
          ? grey_not(grey_accessibilityTrait(UIAccessibilityTraitNotEnabled))
          : grey_accessibilityTrait(UIAccessibilityTraitNotEnabled);
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              chrome_test_util::ButtonWithAccessibilityLabelId(string_id),
              grey_ancestor(grey_kindOfClassName(@"UICollectionView")),
              grey_sufficientlyVisible(), nil)]
      assertWithMatcher:assertion_matcher];
}

// Tests the enabled state of an item.
// `parentMatcher` is the container matcher of the `item`.
// `availability` is the expected availability.
void AssertItemEnabledState(id<GREYMatcher> item,
                            id<GREYMatcher> parentMatcher,
                            bool enabled) {
  id<GREYMatcher> enabledMatcher =
      [ChromeEarlGrey isNewOverflowMenuEnabled]
          // TODO(crbug.com/1285974): grey_userInteractionEnabled doesn't work
          // for SwiftUI views.
          ? grey_not(grey_accessibilityTrait(UIAccessibilityTraitNotEnabled))
          : grey_userInteractionEnabled();
  [[[EarlGrey selectElementWithMatcher:item]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  /*amount=*/200)
      onElementWithMatcher:parentMatcher]
      assertWithMatcher:enabled ? enabledMatcher
                                : grey_accessibilityTrait(
                                      UIAccessibilityTraitNotEnabled)];
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
  // Close the popup menu.
  [ChromeTestCase removeAnyOpenMenusAndInfoBars];
}

// When the IncognitoModeAvailability policy is set to available, the tools
// menu item "New Tab" and "New Incognito Tab" should be enabled.
- (void)testToolsMenuWhenIncognitoAvailable {
  SetIncognitoAvailabiliy(IncognitoAvailability::kAvailable);
  [ChromeEarlGreyUI openToolsMenu];

  AssertItemEnabledState(grey_accessibilityID(kToolsMenuNewTabId),
                         ToolsMenuView(), /*enabled=*/YES);
  AssertItemEnabledState(grey_accessibilityID(kToolsMenuNewIncognitoTabId),
                         ToolsMenuView(), /*enabled=*/YES);
}

// When the IncognitoModeAvailability policy is set to disabled, the tools menu
// item "New Incognito Tab" should be disabled.
- (void)testToolsMenuWhenIncognitoDisabled {
  SetIncognitoAvailabiliy(IncognitoAvailability::kDisabled);
  [ChromeEarlGreyUI openToolsMenu];

  AssertItemEnabledState(grey_accessibilityID(kToolsMenuNewTabId),
                         ToolsMenuView(), /*enabled=*/YES);
  AssertItemEnabledState(grey_accessibilityID(kToolsMenuNewIncognitoTabId),
                         ToolsMenuView(), /*enabled=*/NO);
}

// When the IncognitoModeAvailability policy is set to forced, the tools menu
// item "New Tab" should be disabled.
- (void)testToolsMenuWhenIncognitoOnly {
  SetIncognitoAvailabiliy(IncognitoAvailability::kOnly);
  [ChromeEarlGreyUI openToolsMenu];

  AssertItemEnabledState(grey_accessibilityID(kToolsMenuNewTabId),
                         ToolsMenuView(), /*enabled=*/NO);
  AssertItemEnabledState(grey_accessibilityID(kToolsMenuNewIncognitoTabId),
                         ToolsMenuView(), /*enabled=*/YES);
}

// When the IncognitoModeAvailability policy is set to available, the "New Tab"
// and "New Incognito Tab" items should be enabled in the popup menu triggered
// by long-pressing the tab grid button.
- (void)testTabGridButtonLongPressMenuWhenIncognitoAvailable {
  SetIncognitoAvailabiliy(IncognitoAvailability::kAvailable);
  // Long press the tab grid button.
  [[EarlGrey selectElementWithMatcher:TabGridButton()]
      performAction:grey_longPress()];

  AssertItemEnabled(IDS_IOS_TOOLS_MENU_NEW_TAB, /*enabled=*/true);
  AssertItemEnabled(IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB, /*enabled=*/true);
}

// When the IncognitoModeAvailability policy is set to disabled, the "New
// Incognito Tab" item should be disabled in the popup menu triggered by
// long-pressing the tab grid button.
- (void)testTabGridButtonLongPressMenuWhenIncognitoDisabled {
  SetIncognitoAvailabiliy(IncognitoAvailability::kDisabled);
  // Long press the tab grid button.
  [[EarlGrey selectElementWithMatcher:TabGridButton()]
      performAction:grey_longPress()];

  AssertItemEnabled(IDS_IOS_TOOLS_MENU_NEW_TAB, /*enabled=*/true);
  AssertItemEnabled(IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB, /*enabled=*/false);
}

// When the IncognitoModeAvailability policy is set to forced, the "New Tab"
// item should be disabled in the popup menu triggered by long-pressing the tab
// grid button.
- (void)testTabGridButtonLongPressMenuWhenIncognitoOnly {
  SetIncognitoAvailabiliy(IncognitoAvailability::kOnly);
  // Long press the tab grid button.
  [[EarlGrey selectElementWithMatcher:TabGridButton()]
      performAction:grey_longPress()];

  AssertItemEnabled(IDS_IOS_TOOLS_MENU_NEW_TAB, /*enabled=*/false);
  AssertItemEnabled(IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB, /*enabled=*/true);
}

// TODO(crbug.com/1165655): Add test to new tab long-press menu.

@end
