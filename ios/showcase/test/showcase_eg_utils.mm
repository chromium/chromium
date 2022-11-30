// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/test/showcase_eg_utils.h"

#import "base/mac/foundation_util.h"
#import "ios/showcase/test/showcase_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/common/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Matcher for the back button on screens presented from the Showcase home
// screen.
id<GREYMatcher> BackButton() {
  return grey_allOf(
      grey_anyOf(grey_buttonTitle(@"SC"), grey_buttonTitle(@"back"), nil),
      grey_ancestor(grey_kindOfClass([UINavigationBar class])), nil);
}

// Matcher for the Showcase home screen view.
id<GREYMatcher> HomeScreen() {
  return grey_accessibilityID(@"showcase_home_collection");
}

// Returns the Showcase navigation controller.
UINavigationController* ShowcaseNavigationController() {
  UINavigationController* showcaseNavigationController =
      base::mac::ObjCCastStrict<UINavigationController>(
          [GetAnyKeyWindow() rootViewController]);
  return showcaseNavigationController;
}

}  // namespace

namespace showcase_utils {

void Open(NSString* name) {
  [[EarlGrey selectElementWithMatcher:HomeScreen()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeTop)];
  // Matcher for the UI element that has the accessibility label |name| and is
  // sufficiently visible, so EarlGrey will not attempt to tap a partially
  // hidden UI element.
  id<GREYMatcher> visibleCellWithAccessibilityLabelMatcher = grey_allOf(
      grey_accessibilityLabel(name), grey_sufficientlyVisible(), nil);

  // Disable EarlGrey's NSTimer tracking while scrolling.
  // TODO(crbug.com/1101608): This is a workaround that should be removed once a
  // proper fix lands in EarlGrey.
  double original_interval =
      GREY_CONFIG_DOUBLE(kGREYConfigKeyNSTimerMaxTrackableInterval);
  [[GREYConfiguration sharedConfiguration]
          setValue:@0
      forConfigKey:kGREYConfigKeyNSTimerMaxTrackableInterval];

  [[[EarlGrey selectElementWithMatcher:visibleCellWithAccessibilityLabelMatcher]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:HomeScreen()] performAction:grey_tap()];

  // Restore the original NSTimer max tracking interval.
  [[GREYConfiguration sharedConfiguration]
          setValue:[NSNumber numberWithDouble:original_interval]
      forConfigKey:kGREYConfigKeyNSTimerMaxTrackableInterval];
}

void Close() {
  // Some screens hides the navigation bar. Make sure it is showing.
  ShowcaseNavigationController().navigationBarHidden = NO;
  [[EarlGrey selectElementWithMatcher:BackButton()]
      assertWithMatcher:grey_interactable()];
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:HomeScreen()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

}  // namespace showcase_utils
