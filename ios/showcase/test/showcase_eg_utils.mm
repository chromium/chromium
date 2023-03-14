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

id<GREYMatcher> SearchBar() {
  return grey_accessibilityID(@"showcase_home_search_bar");
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
  Search(name);

  // Matcher for the UI element that has the accessibility label `name` and is
  // sufficiently visible, so EarlGrey will not attempt to tap a partially
  // hidden UI element.
  id<GREYMatcher> visibleCellWithAccessibilityLabelMatcher = grey_allOf(
      grey_accessibilityLabel(name), grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:visibleCellWithAccessibilityLabelMatcher]
      performAction:grey_tap()];
}

void Close() {
  // Some screens hides the navigation bar. Make sure it is showing.
  ShowcaseNavigationController().navigationBarHidden = NO;
  [[EarlGrey selectElementWithMatcher:BackButton()]
      assertWithMatcher:grey_interactable()];
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:HomeScreen()]
      assertWithMatcher:grey_sufficientlyVisible()];
  ClearSearch();
}

void Search(NSString* query) {
  [[EarlGrey selectElementWithMatcher:SearchBar()]
      performAction:grey_replaceText(query)];
}

void ClearSearch() {
  [[EarlGrey selectElementWithMatcher:SearchBar()]
      performAction:grey_replaceText(@"")];
}

}  // namespace showcase_utils
