// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "testing/gtest/ios_enable_coverage.h"

#if !defined(NDEBUG) && BUILDFLAG(IOS_ENABLE_COVERAGE) && \
    TARGET_IPHONE_SIMULATOR
extern "C" void __llvm_profile_set_filename(const char* name);
#endif

namespace coverage_util {

void ConfigureCoverageReportPath() {
// Targets won't build on real devices with BUILDFLAG(IOS_ENABLE_COVERAGE)
// because of llvm library linking issue for arm64 architecture.
#if !defined(NDEBUG) && BUILDFLAG(IOS_ENABLE_COVERAGE) && \
    TARGET_IPHONE_SIMULATOR
  static dispatch_once_t once_token;
  dispatch_once(&once_token, ^{
    // Writes the profraw file to the simulator shared resources directory,
    // where the app has write rights, and will be preserved after app is
    // killed.
    NSString* shared_resources_path =
        NSProcessInfo.processInfo
            .environment[@"SIMULATOR_SHARED_RESOURCES_DIRECTORY"];
    // UUID ensures that there won't be a conflict when multiple apps are
    // launched in one test suite in EG2. %m enables on-line profile merging.
    // %c helps preserve coverage data at crash.
    NSString* file_name = [NSString
        stringWithFormat:@"%@-%%m-%%c.profraw", NSUUID.UUID.UUIDString];
    NSString* file_path =
        [shared_resources_path stringByAppendingPathComponent:file_name];

    // For documentation, see:
    // http://clang.llvm.org/docs/SourceBasedCodeCoverage.html
    __llvm_profile_set_filename(
        [file_path cStringUsingEncoding:NSUTF8StringEncoding]);

    // Print the path for easier retrieval.
    NSLog(@"Coverage data at %@.", file_path);
  });
#endif  // !defined(NDEBUG) && BUILDFLAG(IOS_ENABLE_COVERAGE) &&
        // TARGET_IPHONE_SIMULATOR
}

}  // namespace coverage_util
