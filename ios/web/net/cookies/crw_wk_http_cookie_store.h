// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NET_COOKIES_CRW_WK_HTTP_COOKIE_STORE_H_
#define IOS_WEB_NET_COOKIES_CRW_WK_HTTP_COOKIE_STORE_H_

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

// A WKHTTPCookieStore wrapper which caches the output of getAllCookies call to
// use on subsequent calls, while observing the core WKHTTPCookieStore to
// invalidate the cached copy once the store is updated.
// This class implements a fix for when WKHTTPCookieStore's getAllCookies method
// callback is not called bug, see crbug.com/885218 for details.
// All the methods of CRWWKHTTPCookieStore follow the same rules of the
// wrapped WKHTTPCookieStore.
@interface CRWWKHTTPCookieStore : NSObject

// CRWWKHTTPCookieStore will not retain the WKHTTPCookieStore instance, and it
// will be deleted with the owning WKWebSiteDataStore.
// Note: CookieStore must be set before any web view that uses it is created.
@property(nonatomic, weak) WKHTTPCookieStore* HTTPCookieStore;

// Fetches all stored cookies. If the store didn't change between calls, this
// method will return the cached result of the last call.
// TODO(crbug.com/946171): Remove caching when WKHTTPCookieStore performance bug
// is fixed.
- (void)getAllCookies:(void (^)(NSArray<NSHTTPCookie*>*))completionHandler;

// Sets |cookie| to the store, and invokes |completionHandler| after cookie is
// set.
- (void)setCookie:(NSHTTPCookie*)cookie
    completionHandler:(void (^)(void))completionHandler;

// Deletes |cookie| from the store, and invokes |completionHandler| after cookie
// is deleted.
- (void)deleteCookie:(NSHTTPCookie*)cookie
    completionHandler:(void (^)(void))completionHandler;

@end

#endif  // IOS_WEB_NET_COOKIES_CRW_WK_HTTP_COOKIE_STORE_H_
