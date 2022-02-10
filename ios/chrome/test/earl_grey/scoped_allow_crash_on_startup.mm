// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/test/earl_grey/scoped_allow_crash_on_startup.h"

#include <atomic>

#include "base/check_op.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

std::atomic_int g_instance_count;

Method GetHandlerMethod(const char* class_name) {
  return class_getInstanceMethod(
      objc_getClass(class_name),
      NSSelectorFromString(@"handleCrashUnderSymbol:"));
}

const EDOClientErrorHandler kNopClientErrorHandler = ^(NSError* error) {
  NSLog(@"NOP error handler (allows crash on startup)");
};

}  // namespace

@interface ScopedAllowCrashOnStartup_NopErrorHandler : NSObject
- (void)handleCrashUnderSymbol:(id)unused;
@end

@implementation ScopedAllowCrashOnStartup_NopErrorHandler
- (void)handleCrashUnderSymbol:(id)unused {
  NSLog(@"NOP handleCrashUnderSymbol");
}

@end

// Swizzle away the handleCrashUnderSymbol callback.  Without this, any time
// the host app is intentionally crashed, the test is immediately failed.
ScopedAllowCrashOnStartup::ScopedAllowCrashOnStartup()
    : original_crash_handler_method_(GetHandlerMethod("XCUIApplicationImpl")),
      swizzled_crash_handler_method_(
          GetHandlerMethod("ScopedAllowCrashOnStartup_NopErrorHandler")),
      original_client_error_handler_(
          EDOSetClientErrorHandler(kNopClientErrorHandler)) {
  NSLog(@"Swizzle app crash error handlers");
  method_exchangeImplementations(original_crash_handler_method_,
                                 swizzled_crash_handler_method_);
  CHECK_EQ(++g_instance_count, 1);
}

ScopedAllowCrashOnStartup::~ScopedAllowCrashOnStartup() {
  NSLog(@"Un-swizzle app crash error handlers!");
  EDOSetClientErrorHandler(original_client_error_handler_);
  method_exchangeImplementations(swizzled_crash_handler_method_,
                                 original_crash_handler_method_);
  --g_instance_count;
}

// static
bool ScopedAllowCrashOnStartup::IsActive() {
  return g_instance_count == 1;
}
