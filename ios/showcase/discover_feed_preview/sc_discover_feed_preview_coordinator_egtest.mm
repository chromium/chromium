// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/context_menu/link_preview/link_preview_constants.h"
#import "ios/showcase/test/showcase_eg_utils.h"
#import "ios/showcase/test/showcase_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

namespace {
using ::showcase_utils::Open;
using ::showcase_utils::Close;
}

// Tests for the discover feed preview.
@interface SCDiscoverFeedPreviewTestCase : ShowcaseTestCase
@end

@implementation SCDiscoverFeedPreviewTestCase

- (void)setUp {
  [super setUp];
  Open(@"Link Preview");
}

- (void)tearDown {
  Close();
  [super tearDown];
}

// Tests that the Discover Feed Preview is correctly displaying.
- (void)testDiscoverFeedPreview {
  // Check the URL bar is presented.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kPreviewURLBarIdentifier),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check the origin is set correctly.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kPreviewOriginIdentifier),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_text(@"test.url")];

  // Check the loading bar is presented.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kPreviewProgressBarIdentifier),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check the page content view is presented.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kPreviewWebStateViewIdentifier),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
