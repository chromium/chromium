// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/earl_grey/coverage_utils.h"

#include "base/clang_profiling_buildflags.h"
#import "testing/coverage_util_ios.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#if BUILDFLAG(CLANG_PROFILING)
#include "base/test/clang_profiling.h"
extern "C" void __llvm_profile_reset_counters(void);
#endif

@implementation CoverageUtils

+ (void)configureCoverageReportPath {
  coverage_util::ConfigureCoverageReportPath();
}

+ (void)resetCoverageProfileCounters {
#if BUILDFLAG(CLANG_PROFILING)
  // In this call, the already-dump flag is also reset, so that the same file
  // can be dumped to again.
  __llvm_profile_reset_counters();
#endif  // BUILDFLAG(CLANG_PROFILING)
}

+ (void)writeClangCoverageProfile {
#if BUILDFLAG(CLANG_PROFILING)
  base::WriteClangProfilingProfile();
#endif  // BUILDFLAG(CLANG_PROFILING)
}

@end
