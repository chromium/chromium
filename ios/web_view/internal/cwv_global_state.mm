// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <memory>

#import "base/check_op.h"
#import "base/strings/sys_string_conversions.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "google_apis/google_api_keys.h"
#import "ios/web/public/init/web_main.h"
#import "ios/web_view/internal/browser_state_keyed_service_factories.h"
#import "ios/web_view/internal/cwv_global_state_internal.h"
#import "ios/web_view/internal/web_view_web_client.h"
#import "ios/web_view/internal/web_view_web_main_delegate.h"
#if !defined(CWV_UNIT_TEST)
#import "testing/coverage_util_ios.h"
#endif  // defined(CWV_UNIT_TEST)

@implementation CWVEarlyInitFlags
@end

@implementation CWVGlobalState {
  std::unique_ptr<ios_web_view::WebViewWebClient> _web_client;
  std::unique_ptr<ios_web_view::WebViewWebMainDelegate> _web_main_delegate;
  std::unique_ptr<web::WebMain> _web_main;
  BOOL _isEarlyInitialized;
  BOOL _isStarted;
  NSString* _customUserAgent;
  NSString* _userAgentProduct;
}

+ (instancetype)sharedInstance {
  static CWVGlobalState* globalState = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    globalState = [[CWVGlobalState alloc] init];
  });
  return globalState;
}

- (NSString*)customUserAgent {
  return _customUserAgent;
}

- (void)setCustomUserAgent:(NSString*)customUserAgent {
  _customUserAgent = [customUserAgent copy];
}

- (NSString*)userAgentProduct {
  return _userAgentProduct;
}

- (void)setUserAgentProduct:(NSString*)userAgentProduct {
  _userAgentProduct = [userAgentProduct copy];
}

- (void)setGoogleAPIKey:(NSString*)googleAPIKey
               clientID:(NSString*)clientID
           clientSecret:(NSString*)clientSecret {
  google_apis::InitializeAndOverrideAPIKeyAndOAuthClient(
      base::SysNSStringToUTF8(googleAPIKey), base::SysNSStringToUTF8(clientID),
      base::SysNSStringToUTF8(clientSecret));
}

- (BOOL)isStarted {
#if defined(CWV_UNIT_TEST)
  // Global state initialization is not needed in a unit test environment.
  return YES;
#else
  return _isStarted;
#endif  // defined(CWV_UNIT_TEST)
}

- (BOOL)isEarlyInitialized {
#if defined(CWV_UNIT_TEST)
  // Global state initialization is not needed in a unit test environment.
  return YES;
#else
  return _isEarlyInitialized;
#endif  // defined(CWV_UNIT_TEST)
}

- (void)earlyInitWithFlags:(CWVEarlyInitFlags*)flags {
#if defined(CWV_UNIT_TEST)
  // Global state initialization is not needed in a unit test environment.
#else
  // Set flags before doing anything else.
  CHECK(flags);
  _autofillAcrossIframesEnabled = flags.autofillAcrossIframesEnabled;

  DCHECK([NSThread isMainThread]);

  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    DCHECK(!_isEarlyInitialized);

    // This is for generating coverage data for tests only.
    coverage_util::ConfigureCoverageReportPath();

    _web_client = std::make_unique<ios_web_view::WebViewWebClient>();
    web::SetWebClient(_web_client.get());
    _web_main_delegate =
        std::make_unique<ios_web_view::WebViewWebMainDelegate>();
    web::WebMainParams params(_web_main_delegate.get());
    _web_main = std::make_unique<web::WebMain>(std::move(params));

    _isEarlyInitialized = YES;
  });
#endif  // defined(CWV_UNIT_TEST)
}

- (void)earlyInit {
  [self earlyInitWithFlags:[[CWVEarlyInitFlags alloc] init]];
}

- (void)start {
#if defined(CWV_UNIT_TEST)
  // Global state initialization is not needed in a unit test environment.
#else
  DCHECK([NSThread isMainThread]);

  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    DCHECK(_isEarlyInitialized);
    DCHECK(!_isStarted);

    _web_main->Startup();

    _isStarted = YES;
  });
#endif  // defined(CWV_UNIT_TEST)
}

- (void)stop {
#if defined(CWV_UNIT_TEST)
  // Global state initialization is not needed in a unit test environment.
#else
  DCHECK([NSThread isMainThread]);

  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    // In reverse order to creation.
    _web_main.reset();
    _web_main_delegate.reset();
    _web_client.reset();
  });
#endif  // defined(CWV_UNIT_TEST)
}

@end
