// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/gaia_auth_fetcher_ios_ns_url_session_bridge.h"

#import <Foundation/Foundation.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/signin/feature_flags.h"
#include "ios/net/cookies/system_cookie_util.h"
#include "ios/web/common/features.h"
#include "ios/web/public/browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface GaiaAuthFetcherIOSURLSessionDelegate
    : NSObject <NSURLSessionTaskDelegate>

// Gaia auth fetcher bridge.
@property(nonatomic, assign) GaiaAuthFetcherIOSNSURLSessionBridge* bridge;
// Session for the multilogin request.
@property(nonatomic, strong) NSURLSession* requestSession;

@end

@implementation GaiaAuthFetcherIOSURLSessionDelegate

@synthesize bridge = _bridge;
@synthesize requestSession = _requestSession;

#pragma mark - NSURLSessionTaskDelegate

- (void)URLSession:(NSURLSession*)session
                          task:(NSURLSessionTask*)task
    willPerformHTTPRedirection:(NSHTTPURLResponse*)response
                    newRequest:(NSURLRequest*)request
             completionHandler:(void (^)(NSURLRequest*))completionHandler {
  // If there is a redirect, the cookies from the redirect need to be stored.
  DCHECK(self.requestSession == session);
  if (self.bridge) {
    self.bridge->SetCanonicalCookiesFromResponse(response);
    completionHandler(request);
  } else {
    // No need to continue the redirect if there is no more bridge instance.
    completionHandler(NULL);
  }
}

#pragma mark - Private

- (void)requestCompletedWithData:(NSData*)data
                        response:(NSURLResponse*)response
                           error:(NSError*)error {
  if (!self.bridge)
    return;
  NSHTTPURLResponse* responseWithHeaders =
      base::mac::ObjCCastStrict<NSHTTPURLResponse>(response);
  if (error) {
    self.bridge->OnURLFetchFailure(net::ERR_FAILED,
                                   responseWithHeaders.statusCode);
  } else {
    self.bridge->SetCanonicalCookiesFromResponse(responseWithHeaders);
    NSString* result = [[NSString alloc] initWithData:data
                                             encoding:NSUTF8StringEncoding];
    self.bridge->OnURLFetchSuccess(base::SysNSStringToUTF8(result),
                                   responseWithHeaders.statusCode);
  }
}

@end

GaiaAuthFetcherIOSNSURLSessionBridge::GaiaAuthFetcherIOSNSURLSessionBridge(
    GaiaAuthFetcherIOSBridge::GaiaAuthFetcherIOSBridgeDelegate* delegate,
    web::BrowserState* browser_state)
    : GaiaAuthFetcherIOSBridge(delegate, browser_state) {
  DCHECK(base::FeatureList::IsEnabled(kUseNSURLSessionForGaiaSigninRequests));
  url_session_delegate_ = [[GaiaAuthFetcherIOSURLSessionDelegate alloc] init];
  url_session_delegate_.bridge = this;
}

GaiaAuthFetcherIOSNSURLSessionBridge::~GaiaAuthFetcherIOSNSURLSessionBridge() {
  url_session_delegate_.bridge = nullptr;
}

void GaiaAuthFetcherIOSNSURLSessionBridge::FetchPendingRequest() {
  network::mojom::CookieManager* cookie_manager =
      GetBrowserState()->GetCookieManager();
  net::CookieOptions options;
  options.set_include_httponly();
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT);
  cookie_manager->GetCookieList(
      GetRequest().url, options,
      base::BindOnce(
          &GaiaAuthFetcherIOSNSURLSessionBridge::FetchPendingRequestWithCookies,
          base::Unretained(this)));
}

void GaiaAuthFetcherIOSNSURLSessionBridge::Cancel() {
  [url_session_data_task_ cancel];
  OnURLFetchFailure(net::ERR_ABORTED, 0);
}

void GaiaAuthFetcherIOSNSURLSessionBridge::SetCanonicalCookiesFromResponse(
    NSHTTPURLResponse* response) {
  NSArray* cookies =
      [NSHTTPCookie cookiesWithResponseHeaderFields:response.allHeaderFields
                                             forURL:response.URL];
  network::mojom::CookieManager* cookie_manager =
      GetBrowserState()->GetCookieManager();
  for (NSHTTPCookie* cookie : cookies) {
    net::CookieOptions options;
    options.set_include_httponly();
    // Permit it to set a SameSite cookie if it wants to.
    options.set_same_site_cookie_context(
        net::CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT);
    cookie_manager->SetCanonicalCookie(
        net::CanonicalCookieFromSystemCookie(cookie, base::Time::Now()),
        base::SysNSStringToUTF8(response.URL.scheme), options,
        base::DoNothing());
  }
}

void GaiaAuthFetcherIOSNSURLSessionBridge::FetchPendingRequestWithCookies(
    const net::CookieStatusList& cookies_with_statuses,
    const net::CookieStatusList& excluded_cookies) {
  DCHECK(!url_session_);
  url_session_ = CreateNSURLSession(url_session_delegate_);
  url_session_delegate_.requestSession = url_session_;
  DCHECK(!url_session_data_task_);
  __weak __typeof(url_session_delegate_) weakDelegate = url_session_delegate_;
  url_session_data_task_ =
      [url_session_ dataTaskWithRequest:GetNSURLRequest()
                      completionHandler:^(NSData* data, NSURLResponse* response,
                                          NSError* error) {
                        [weakDelegate requestCompletedWithData:data
                                                      response:response
                                                         error:error];
                      }];
  NSMutableArray* http_cookies =
      [[NSMutableArray alloc] initWithCapacity:cookies_with_statuses.size()];
  for (const auto& cookie_with_status : cookies_with_statuses) {
    [http_cookies addObject:net::SystemCookieFromCanonicalCookie(
                                cookie_with_status.cookie)];
  }
  [url_session_.configuration.HTTPCookieStorage
      storeCookies:http_cookies
           forTask:url_session_data_task_];

  [url_session_data_task_ resume];
}

NSURLSession* GaiaAuthFetcherIOSNSURLSessionBridge::CreateNSURLSession(
    id<NSURLSessionTaskDelegate> url_session_delegate) {
  NSURLSessionConfiguration* session_configuration =
      NSURLSessionConfiguration.ephemeralSessionConfiguration;
  session_configuration.HTTPShouldSetCookies = YES;
  return [NSURLSession sessionWithConfiguration:session_configuration
                                       delegate:url_session_delegate
                                  delegateQueue:NSOperationQueue.mainQueue];
}
