// Copyright 2019 The Chromium Authors
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

// WKHTTPCookieStore will be accessed via `websiteDataStore`.
// CRWWKHTTPCookieStore will not retain the WKWebsiteDataStore and
// WKHTTPCookieStore instances, and they will be deleted while tearing down an
// application.
// Note: the data store must be set before any web view that uses it is created.
@property(nonatomic, weak) WKWebsiteDataStore* websiteDataStore;

// Fetches all stored cookies. If the store didn't change between calls, this
// method will return the cached result of the last call.
// TODO(crbug.com/40620220): Remove caching when WKHTTPCookieStore performance
// bug is fixed.
- (void)getAllCookies:(void (^)(NSArray<NSHTTPCookie*>*))completionHandler;

// Sets `cookie` to the store, and invokes `completionHandler` after cookie is
// set.
- (void)setCookie:(NSHTTPCookie*)cookie
    completionHandler:(void (^)(void))completionHandler;

// Deletes `cookie` from the store, and invokes `completionHandler` after cookie
// is deleted.
- (void)deleteCookie:(NSHTTPCookie*)cookie
    completionHandler:(void (^)(void))completionHandler;

// Deletes all cookies from the store, and invokes `completionHandler` after
// they have all been deleted.
- (void)clearCookies:(void (^)(void))completionHandler;

@end

#endif  // IOS_WEB_NET_COOKIES_CRW_WK_HTTP_COOKIE_STORE_H_
