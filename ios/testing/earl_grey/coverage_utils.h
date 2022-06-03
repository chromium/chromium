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

// Resets coverage counter and already-dumped flag so that incremental coverage
// data can be dumped to the same raw coverage data file. This should be called
// only once in between two write coverage data calls.
+ (void)resetCoverageProfileCounters;

// Writes the raw coverage data to previously configured report path.
+ (void)writeClangCoverageProfile;

@end

#endif  // IOS_TESTING_EARL_GREY_COVERAGE_UTILS_H_
