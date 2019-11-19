// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/earl_grey/coverage_utils.h"

#include "testing/coverage_util_ios.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CoverageUtils

// On first call in a debug build with IOS_ENABLE_COVERAGE enabled, will set the
// filename of the coverage file. Will do nothing on subsequent calls, but is
// safe to call.
+ (void)configureCoverageReportPath {
  coverage_util::ConfigureCoverageReportPath();
}

@end