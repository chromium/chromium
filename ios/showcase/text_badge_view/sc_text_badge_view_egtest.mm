// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/test/showcase_eg_utils.h"
#import "ios/showcase/test/showcase_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

namespace {
using ::showcase_utils::Open;
using ::showcase_utils::Close;
}

@interface SCTextBadgeViewTestCase : ShowcaseTestCase
@end

@implementation SCTextBadgeViewTestCase

// Tests that the accessibility label matches the display text.
- (void)testTextBadgeAccessibilityLabel {
  Open(@"TextBadgeView");
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"TEXT")]
      assertWithMatcher:grey_notNil()];
  Close();
}

@end
