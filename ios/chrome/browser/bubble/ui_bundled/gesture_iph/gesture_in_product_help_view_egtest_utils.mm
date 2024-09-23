// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bubble/ui_bundled/gesture_iph/gesture_in_product_help_view_egtest_utils.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_constants.h"
#import "ios/chrome/browser/bubble/ui_bundled/gesture_iph/gesture_in_product_help_constants.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

namespace {

// FirstRunRecency key, should match the one in `system_flags`.
NSString* kFirstRunRecencyKey = @"FirstRunRecency";

// Constant for timeout while waiting for a gestural IPH to appear or disappear.
const base::TimeDelta kWaitForGestureIPHTimeOut = base::Seconds(2);

}  // namespace

void MakeFirstRunRecent() {
  [ChromeEarlGrey setUserDefaultsObject:@59 forKey:kFirstRunRecencyKey];
}

void ResetFirstRunRecency() {
  [ChromeEarlGrey removeUserDefaultsObjectForKey:kFirstRunRecencyKey];
}

void RelaunchWithIPHFeature(NSString* feature, BOOL safari_switcher) {
  // Enable the flag to ensure the IPH triggers.
  AppLaunchConfiguration config = AppLaunchConfiguration();
  config.iph_feature_enabled = base::SysNSStringToUTF8(feature);
  if (safari_switcher) {
    config.additional_args.push_back("--enable-features=IPHForSafariSwitcher");
    // Force the conditions that allow the iph to show.
    config.additional_args.push_back("-ForceExperienceForDeviceSwitcher");
    config.additional_args.push_back("SyncedAndFirstDevice");
  }
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Cannot setup histogram tester.");
}

void AssertGestureIPHVisibleWithDismissAction(NSString* description,
                                              void (^action)(void)) {
  // Disable scoped synchronization to perform checks with animation running.
  ScopedSynchronizationDisabler sync_disabler;
  BOOL visibility =
      [ChromeEarlGrey
          testUIElementAppearanceWithMatcher:
              grey_accessibilityID(kGestureInProductHelpViewBackgroundAXId)
                                     timeout:kWaitForGestureIPHTimeOut] &&
      [ChromeEarlGrey
          testUIElementAppearanceWithMatcher:
              grey_accessibilityID(kGestureInProductHelpViewBubbleAXId)
                                     timeout:kWaitForGestureIPHTimeOut];
  GREYAssertTrue(visibility, description);
  if (action) {
    action();
  }
}

void AssertGestureIPHInvisible(NSString* description) {
  // Disable scoped synchronization to perform checks with animation running.
  ScopedSynchronizationDisabler sync_disabler;
  id<GREYInteraction> selectIPH =
      [EarlGrey selectElementWithMatcher:
                    grey_allOf(grey_ancestor(grey_accessibilityID(
                                   kGestureInProductHelpViewBackgroundAXId)),
                               grey_ancestor(grey_accessibilityID(
                                   kGestureInProductHelpViewBubbleAXId)),
                               nil)];

  ConditionBlock iphInvisible = ^{
    NSError* error = nil;
    [selectIPH assertWithMatcher:grey_nil() error:&error];
    return error == nil;
  };

  bool matched = base::test::ios::WaitUntilConditionOrTimeout(
      kWaitForGestureIPHTimeOut, iphInvisible);
  GREYAssertTrue(matched, description);

  // Also make sure it doesn't re-appear.
  ConditionBlock iphVisible = ^{
    NSError* error = nil;
    [selectIPH assertWithMatcher:grey_sufficientlyVisible() error:&error];
    return error == nil;
  };
  matched = base::test::ios::WaitUntilConditionOrTimeout(
      kWaitForGestureIPHTimeOut, iphVisible);
  GREYAssertFalse(matched, description);
}

void TapDismissButton() {
  ScopedSynchronizationDisabler sync_disabler;
  id<GREYMatcher> dismissButton = grey_allOf(
      grey_accessibilityID(kGestureInProductHelpViewDismissButtonAXId),
      grey_interactable(), nil);
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:dismissButton
                                  timeout:kWaitForGestureIPHTimeOut];
  [[EarlGrey selectElementWithMatcher:dismissButton] performAction:grey_tap()];
}

void SwipeIPHInDirection(GREYDirection direction, BOOL edge_swipe) {
  ScopedSynchronizationDisabler sync_disabler;

  CGFloat xOriginStartPercentage = 0.5f;
  CGFloat yOriginStartPercentage = 0.5f;
  if (edge_swipe) {
    switch (direction) {
      case kGREYDirectionUp:
        yOriginStartPercentage = 0.99f;
        break;
      case kGREYDirectionDown:
        yOriginStartPercentage = 0;
        break;
      case kGREYDirectionLeft:
        xOriginStartPercentage = 0.99f;
        break;
      case kGREYDirectionRight:
        xOriginStartPercentage = 0;
        break;
    }
  }
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kGestureInProductHelpViewBackgroundAXId)]
      performAction:grey_swipeFastInDirectionWithStartPoint(
                        direction, xOriginStartPercentage,
                        yOriginStartPercentage)];
}

void ExpectHistogramEmittedForIPHDismissal(IPHDismissalReasonType reason) {
  NSString* dismissalHistogramName =
      base::SysUTF8ToNSString(kUMAGesturalIPHDismissalReason);
  NSError* error = [MetricsAppInterface expectCount:1
                                          forBucket:static_cast<int>(reason)
                                       forHistogram:dismissalHistogramName];
  if (!error) {
    error = [MetricsAppInterface expectTotalCount:1
                                     forHistogram:dismissalHistogramName];
  }
  GREYAssertNil(error, error.description);
}
