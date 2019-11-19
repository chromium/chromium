// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/test/showcase_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Tests for the core of the Showcase app.
@interface ShowcaseCoreTestCase : ShowcaseTestCase
@end

@implementation ShowcaseCoreTestCase

// Tests that Showcase title appears after launch.
- (void)testLaunch {
  [[EarlGrey selectElementWithMatcher:grey_text(@"SC")]
      assertWithMatcher:grey_notNil()];
}

@end
