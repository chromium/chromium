// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_TESTING_EARL_GREY_COVERAGE_UTILS_H_
#define IOS_TESTING_EARL_GREY_COVERAGE_UTILS_H_

#import <Foundation/Foundation.h>

@interface CoverageUtils : NSObject

// On first call in a debug build with IOS_ENABLE_COVERAGE enabled, will set the
// filename of the coverage file. Will do nothing on subsequent calls, but is
// safe to call.
+ (void)configureCoverageReportPath;

@end

#endif  // IOS_TESTING_EARL_GREY_COVERAGE_UTILS_H_
