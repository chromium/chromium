// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/test/showcase_test_case.h"

#import "base/mac/foundation_util.h"
#import "ios/showcase/test/showcase_test_case_app_interface.h"
#import "ios/testing/earl_grey/coverage_utils.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ShowcaseTestCase

+ (void)setUpForTestCase {
  [super setUpForTestCase];
  [ShowcaseTestCase setUpHelper];
}

+ (void)setUpHelper {
  [CoverageUtils configureCoverageReportPath];
}

- (void)setUp {
  [super setUp];
  [ShowcaseTestCaseAppInterface setupUI];
}

@end
