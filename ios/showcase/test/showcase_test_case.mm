// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/test/showcase_test_case.h"

#include "base/logging.h"
#import "base/mac/foundation_util.h"
#import "ios/showcase/test/showcase_test_case_app_interface.h"
#import "ios/testing/earl_grey/coverage_utils.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#if defined(CHROME_EARL_GREY_2)
GREY_STUB_CLASS_IN_APP_MAIN_QUEUE(ShowcaseTestCaseAppInterface)
#endif  // defined(CHROME_EARL_GREY_2)

@implementation ShowcaseTestCase

#if defined(CHROME_EARL_GREY_1)
+ (void)setUp {
  [super setUp];
  [ShowcaseTestCase setUpHelper];
}
#elif defined(CHROME_EARL_GREY_2)
+ (void)setUpForTestCase {
  [super setUpForTestCase];
  [ShowcaseTestCase setUpHelper];
}
#endif  // CHROME_EARL_GREY_2

+ (void)setUpHelper {
  [CoverageUtils configureCoverageReportPath];
}

- (void)setUp {
  [super setUp];
  [ShowcaseTestCaseAppInterface setupUI];
}

@end
