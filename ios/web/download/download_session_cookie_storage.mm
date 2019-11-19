// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/download/download_session_cookie_storage.h"

#include "ios/net/cookies/system_cookie_util.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation DownloadSessionCookieStorage

@synthesize cookies = _cookies;
@synthesize cookieAcceptPolicy = _cookieAcceptPolicy;

- (instancetype)init {
  if (self = [super init]) {
    _cookies = [[NSMutableArray alloc] init];
  }
  return self;
}

// This method assumes that it will be called with valid cookies, with no
// repeated cookies and no expired cookies.
- (void)setCookie:(NSHTTPCookie*)cookie {
  if (self.cookieAcceptPolicy == NSHTTPCookieAcceptPolicyNever) {
    return;
  }
  [_cookies addObject:cookie];
}

- (void)deleteCookie:(NSHTTPCookie*)cookie {
  NOTREACHED();
}

- (nullable NSArray<NSHTTPCookie*>*)cookiesForURL:(NSURL*)URL {
  NSMutableArray<NSHTTPCookie*>* result = [NSMutableArray array];
  GURL gURL = net::GURLWithNSURL(URL);
  // TODO(crbug.com/1018272): Compute the cookie access semantic, and update
  // |options| with it.
  net::CookieOptions options = net::CookieOptions::MakeAllInclusive();
  net::CookieAccessSemantics cookieAccessSemantics =
      net::CookieAccessSemantics::LEGACY;
  if (@available(iOS 13, *)) {
    // Using |UNKNOWN| semantics to allow the experiment to switch between non
    // legacy (where cookies that don't have a specific same-site access policy
    // and not secure will not be included), and legacy mode.
    cookieAccessSemantics = net::CookieAccessSemantics::UNKNOWN;
  }
  for (NSHTTPCookie* cookie in self.cookies) {
    net::CanonicalCookie canonical_cookie =
        net::CanonicalCookieFromSystemCookie(cookie, base::Time());
    if (canonical_cookie
            .IncludeForRequestURL(gURL, options, cookieAccessSemantics)
            .IsInclude())
      [result addObject:cookie];
  }
  return [result copy];
}

- (nullable NSArray<NSHTTPCookie*>*)cookies {
  return [_cookies copy];
}

- (void)setCookies:(NSArray<NSHTTPCookie*>*)cookies
             forURL:(nullable NSURL*)URL
    mainDocumentURL:(nullable NSURL*)mainDocumentURL {
  if (self.cookieAcceptPolicy == NSHTTPCookieAcceptPolicyNever) {
    return;
  }
  if (self.cookieAcceptPolicy ==
      NSHTTPCookieAcceptPolicyOnlyFromMainDocumentDomain) {
    if (!mainDocumentURL.host ||
        ![[@"." stringByAppendingString:URL.host]
            hasSuffix:[@"." stringByAppendingString:mainDocumentURL.host]]) {
      return;
    }
  }
  [_cookies addObjectsFromArray:cookies];
}

- (void)storeCookies:(NSArray<NSHTTPCookie*>*)cookies
             forTask:(NSURLSessionTask*)task {
  [self setCookies:cookies
               forURL:task.currentRequest.URL
      mainDocumentURL:task.currentRequest.mainDocumentURL];
}

- (void)getCookiesForTask:(NSURLSessionTask*)task
        completionHandler:(void (^)(NSArray<NSHTTPCookie*>* _Nullable cookies))
                              completionHandler {
  if (completionHandler)
    completionHandler([self cookiesForURL:task.currentRequest.URL]);
}

@end
