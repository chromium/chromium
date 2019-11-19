// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/test/showcase_eg_utils.h"

#import "base/mac/foundation_util.h"
#import "ios/showcase/test/showcase_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Matcher for the back button on screens presented from the Showcase home
// screen.
id<GREYMatcher> BackButton() {
  return grey_allOf(grey_kindOfClass([UIButton class]),
                    grey_ancestor(grey_kindOfClass([UINavigationBar class])),
                    grey_accessibilityLabel(@"SC"), nil);
}

// Matcher for the Showcase home screen view.
id<GREYMatcher> HomeScreen() {
  return grey_accessibilityID(@"showcase_home_collection");
}

// Returns the Showcase navigation controller.
UINavigationController* ShowcaseNavigationController() {
  UINavigationController* showcaseNavigationController =
      base::mac::ObjCCastStrict<UINavigationController>(
          [[[UIApplication sharedApplication] keyWindow] rootViewController]);
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
  [[[EarlGrey selectElementWithMatcher:visibleCellWithAccessibilityLabelMatcher]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:HomeScreen()] performAction:grey_tap()];
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
