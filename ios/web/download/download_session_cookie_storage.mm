// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/download/download_session_cookie_storage.h"

#import "base/notreached.h"
#import "ios/net/cookies/system_cookie_util.h"
#import "net/base/apple/url_conversions.h"
#import "net/cookies/canonical_cookie.h"
#import "net/cookies/cookie_constants.h"

@implementation DownloadSessionCookieStorage {
  __strong NSMutableArray<NSHTTPCookie*>* _cookies;
  NSHTTPCookieAcceptPolicy _cookieAcceptPolicy;
}

- (instancetype)init {
  return [self initWithCookies:nil
            cookieAcceptPolicy:NSHTTPCookieAcceptPolicyAlways];
}

- (instancetype)initWithCookies:(NSArray<NSHTTPCookie*>*)cookies
             cookieAcceptPolicy:(NSHTTPCookieAcceptPolicy)cookieAcceptPolicy {
  if ((self = [super init])) {
    _cookieAcceptPolicy = cookieAcceptPolicy;
    if (cookies && _cookieAcceptPolicy != NSHTTPCookieAcceptPolicyNever) {
      _cookies = [cookies mutableCopy];
    } else {
      _cookies = [[NSMutableArray alloc] init];
    }
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
  NOTREACHED_IN_MIGRATION();
}

- (NSArray<NSHTTPCookie*>*)cookiesForURL:(NSURL*)URL {
  NSMutableArray<NSHTTPCookie*>* result = [NSMutableArray array];
  GURL gURL = net::GURLWithNSURL(URL);
  // TODO(crbug.com/40104865): Compute the cookie access semantic, and update
  // `options` with it.
  net::CookieOptions options = net::CookieOptions::MakeAllInclusive();
  net::CookieAccessSemantics cookieAccessSemantics =
      net::CookieAccessSemantics::LEGACY;
  // No extra trustworthy URLs.
  bool delegate_treats_url_as_trustworthy = false;

  // Using `UNKNOWN` semantics to allow the experiment to switch between non
  // legacy (where cookies that don't have a specific same-site access policy
  // and not secure will not be included), and legacy mode.
  cookieAccessSemantics = net::CookieAccessSemantics::UNKNOWN;

  net::CookieAccessParams params = {cookieAccessSemantics,
                                    delegate_treats_url_as_trustworthy};
  for (NSHTTPCookie* cookie in self.cookies) {
    std::unique_ptr<net::CanonicalCookie> canonical_cookie =
        net::CanonicalCookieFromSystemCookie(cookie, base::Time());
    if (canonical_cookie->IncludeForRequestURL(gURL, options, params)
            .status.IsInclude())
      [result addObject:cookie];
  }
  return [result copy];
}

- (void)setCookies:(NSArray<NSHTTPCookie*>*)cookies
             forURL:(NSURL*)URL
    mainDocumentURL:(NSURL*)mainDocumentURL {
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

#pragma mark - NSHTTPCookieStorage Properties

- (NSArray<NSHTTPCookie*>*)cookies {
  return [_cookies copy];
}

- (NSHTTPCookieAcceptPolicy)cookieAcceptPolicy {
  return _cookieAcceptPolicy;
}

- (void)setCookieAcceptPolicy:(NSHTTPCookieAcceptPolicy)cookieAcceptPolicy {
  _cookieAcceptPolicy = cookieAcceptPolicy;
}

@end
