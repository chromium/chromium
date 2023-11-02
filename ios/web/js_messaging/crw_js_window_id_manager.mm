// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/crw_js_window_id_manager.h"

#import <ostream>

#import "base/dcheck_is_on.h"
#import "base/logging.h"
#import "base/metrics/histogram_macros.h"
#import "base/notreached.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "crypto/random.h"
#import "ios/web/js_messaging/page_script_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Number of random bytes in unique key for window ID. The length of the
// window ID will be twice this number, as it is hexadecimal encoded.
const size_t kUniqueKeyLength = 16;

#if DCHECK_IS_ON()
// The time in seconds which is determined to be a long wait for the injection
// of the window ID. The wait time will be logged if the time exceeds this
// value.
const double kSignificantInjectionTime = 0.1;

// Returns whether `error` represents a failure to execute JavaScript due to
// JavaScript execution being disallowed.
bool IsJavaScriptExecutionProhibitedError(NSError* error) {
  return error.code == WKErrorJavaScriptExceptionOccurred &&
         [@"Cannot execute JavaScript in this document"
             isEqualToString:error.userInfo[@"WKJavaScriptExceptionMessage"]];
}
#endif

}  // namespace

@interface CRWJSWindowIDManager () {
  // Web view used for script evaluation to inject window ID.
  WKWebView* _webView;
  // Backs up property with the same name.
  NSString* _windowID;
}

// Returns a string of randomized ASCII characters.
+ (NSString*)newUniqueKey;

@end

@implementation CRWJSWindowIDManager

- (NSString*)windowID {
  return _windowID;
}

- (instancetype)initWithWebView:(WKWebView*)webView {
  if ((self = [super init])) {
    _webView = webView;
    _windowID = [[self class] newUniqueKey];
  }
  return self;
}

- (void)inject {
  [self injectWithStartTime:base::TimeTicks::Now()];
}

- (void)injectWithStartTime:(base::TimeTicks)startTime {
  _windowID = [[self class] newUniqueKey];
  NSString* script = [web::GetPageScript(@"window_id")
      stringByReplacingOccurrencesOfString:@"$(WINDOW_ID)"
                                withString:_windowID];
  // WKUserScript for message API may not be injected yet. Make windowID script
  // return boolean indicating whether the injection was successful.
  NSString* scriptWithResult = [NSString
      stringWithFormat:@"if (!window.__gCrWeb || !window.__gCrWeb.message) "
                       @"{false; } else { %@; true; }",
                       script];

  __weak CRWJSWindowIDManager* weakSelf = self;
  [_webView evaluateJavaScript:scriptWithResult
             completionHandler:^(id result, NSError* error) {
               CRWJSWindowIDManager* strongSelf = weakSelf;
               if (!strongSelf)
                 return;
               if (error) {
#if DCHECK_IS_ON()
                 BOOL isExpectedError =
                     error.code == WKErrorWebViewInvalidated ||
                     error.code == WKErrorWebContentProcessTerminated ||
                     IsJavaScriptExecutionProhibitedError(error);
                 DCHECK(isExpectedError)
                     << base::SysNSStringToUTF8(error.domain) << "-"
                     << error.code << " "
                     << base::SysNSStringToUTF16(scriptWithResult);
#endif
                 return;
               }

               // If `result` is an incorrect type, do not check its value.
               // Also do not attempt to re-inject scripts as it may lead to
               // endless recursion attempting to inject the scripts correctly.
               if (result && CFBooleanGetTypeID() !=
                                 CFGetTypeID((__bridge CFTypeRef)result)) {
                 NOTREACHED();
                 return;
               }

               if (![result boolValue]) {
                 // WKUserScript has not been injected yet. Retry window id
                 // injection, because it is critical for the system to
                 // function.
                 [strongSelf injectWithStartTime:startTime];
               } else {
                 base::TimeDelta elapsed = base::TimeTicks::Now() - startTime;
#if DCHECK_IS_ON()
                 DLOG_IF(WARNING,
                         elapsed.InSecondsF() > kSignificantInjectionTime)
                     << "Elapsed time for windowID injection: " << elapsed;
#endif
                 UMA_HISTOGRAM_TIMES("IOS.WindowIDInjection.ElapsedTime",
                                     elapsed);
               }
             }];
}

#pragma mark - Private

+ (NSString*)newUniqueKey {
  char randomBytes[kUniqueKeyLength];
  crypto::RandBytes(randomBytes, kUniqueKeyLength);
  std::string result = base::HexEncode(randomBytes, kUniqueKeyLength);
  return base::SysUTF8ToNSString(result);
}

@end
