// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/scoped_allow_crash_on_startup.h"

#import <atomic>

#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/third_party/earl_grey2/src/CommonLib/Assertion/GREYFatalAsserts.h"

namespace {

std::atomic_int g_instance_count;

// Uses `swizzler` to replace the implementation of `originalMethodName` in
// `originalClassName` with that of `swizzledMethodName` of `swizzledClassName`.
// Asserts that the replacement was successful.
void SwizzleMethod(GREYSwizzler* swizzler,
                   NSString* originalClassName,
                   NSString* originalMethodName,
                   NSString* swizzledClassName,
                   NSString* swizzledMethodName) {
  Class originalClass = NSClassFromString(originalClassName);
  Class swizzledClass = NSClassFromString(swizzledClassName);
  SEL originalMethod = NSSelectorFromString(originalMethodName);
  SEL swizzledMethod = NSSelectorFromString(swizzledMethodName);
  IMP swizzledImpl = [swizzledClass instanceMethodForSelector:swizzledMethod];
  BOOL swizzled = [swizzler swizzleClass:originalClass
                       addInstanceMethod:swizzledMethod
                      withImplementation:swizzledImpl
            andReplaceWithInstanceMethod:originalMethod];
  GREYFatalAssertWithMessage(swizzled, @"Failed to swizzle %@-%@",
                             originalClassName, originalMethodName);
}

// NOP implementation of EDOClientErrorHandler.
const EDOClientErrorHandler kNopClientErrorHandler = ^(NSError* error) {
  NSLog(@"NOP EDO client error handler");
};

// NOP implementation of GREYHostApplicationCrashHandler.
const GREYHostApplicationCrashHandler kHostApplicationCrashHandler = ^{
  NSLog(@"NOP host app crash handler");
};

}  // namespace

// Helper class to hold swizzle-able instance method implementations.
@interface ScopedAllowCrashOnStartup_SwizzleHelper : NSObject

// XCUIApplicationImpl-handleCrashUnderSymbol
- (void)swizzledHandleCrashUnderSymbol:(id)unused;

// XCTestCase-recordIssue
- (void)swizzledRecordIssue:(XCTIssue*)issue;

@end

@implementation ScopedAllowCrashOnStartup_SwizzleHelper

// XCUIApplicationImpl-handleCrashUnderSymbol
- (void)swizzledHandleCrashUnderSymbol:(id)unused {
  NSLog(@"NOP handleCrashUnderSymbol");
}

// XCTestCase-recordIssue
- (void)swizzledRecordIssue:(XCTIssue*)issue {
  NSLog(@"----------- captured issue! ------------");
  NSString* description = issue.compactDescription;
  NSLog(@"Description: %@", description);
  if ([description
          containsString:@"Couldn’t communicate with a helper application. Try "
                         @"your operation again. If that fails, quit and "
                         @"relaunch the application and try again."] ||
      [description containsString:@" crashed in "] ||
      [description containsString:@"Failed to get matching snapshot"]) {
    // Ignore these exceptions, since we expect to crash.
    return;
  }
  INVOKE_ORIGINAL_IMP1(void, @selector(swizzledRecordIssue:), issue);
}
@end

ScopedAllowCrashOnStartup::ScopedAllowCrashOnStartup() {
  GREYFatalAssertWithMessage(
      ++g_instance_count == 1,
      @"At most one ScopedAllowCrashOnStartup instance may exist at any time.");
  swizzler_ = [[GREYSwizzler alloc] init];

  NSLog(@"Swizzle/stub app crash error handlers");
  SwizzleMethod(swizzler_, @"XCUIApplicationImpl", @"handleCrashUnderSymbol:",
                @"ScopedAllowCrashOnStartup_SwizzleHelper",
                @"swizzledHandleCrashUnderSymbol:");
  SwizzleMethod(swizzler_, @"XCTestCase",
                @"recordIssue:", @"ScopedAllowCrashOnStartup_SwizzleHelper",
                @"swizzledRecordIssue:");
  original_client_error_handler_ =
      EDOSetClientErrorHandler(kNopClientErrorHandler);
  [EarlGrey setHostApplicationCrashHandler:kHostApplicationCrashHandler];
}

ScopedAllowCrashOnStartup::~ScopedAllowCrashOnStartup() {
  NSLog(@"Restore app crash error handlers!");
  [EarlGrey setHostApplicationCrashHandler:nil];
  EDOSetClientErrorHandler(original_client_error_handler_);
  [swizzler_ resetAll];
  GREYFatalAssertWithMessage(--g_instance_count == 0,
                             @"Destroying singleton ScopedAllowCrashOnStartup "
                             @"should bring instance count to 0");
}

// static
bool ScopedAllowCrashOnStartup::IsActive() {
  return g_instance_count != 0;
}
