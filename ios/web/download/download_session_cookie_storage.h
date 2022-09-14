// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_DOWNLOAD_DOWNLOAD_SESSION_COOKIE_STORAGE_H_
#define IOS_WEB_DOWNLOAD_DOWNLOAD_SESSION_COOKIE_STORAGE_H_

#include <Foundation/Foundation.h>

// Cookie Storage that only save cookies in memory. It assumes that all cookies
// set calls will have valid cookies and conform to the global cookies accept
// policy. This cookie store class expects setting & getting cookies methods
// calls and setting cookies accept policy calls. Other methods of the class are
// no op.
// This Cookie store is solely used by the download session, which will only
// retrieve cookies using `cookiesForURL:` when creating the retrieve request.
// After that these cookies are not useful, and it'll be safe to discard them
// and only have the version kept by the WebSiteDataStore internal cookie store.
// The reason why an instance of NSHTTPCookieStorage class (shared or newly
// created) can not be used directly in the download session is that it takes
// too long blocking UI thread when setting large number of cookies.
@interface DownloadSessionCookieStorage : NSHTTPCookieStorage

// Initialises the instance with the `cookies` and `cookieAcceptPolicy`. The
// `cookies` must be valid and without duplicates. If `cookiAcceptPolicy` is
// `NSHTTPCookieAcceptPolicyNever`, then no cookies are set.
- (instancetype)initWithCookies:(NSArray<NSHTTPCookie*>*)cookies
             cookieAcceptPolicy:(NSHTTPCookieAcceptPolicy)cookieAcceptPolicy
    NS_DESIGNATED_INITIALIZER;

// Convenience initialiser identical to calling:
// [DownloadSessionCookieStorage alloc]
//     initWithCookies:nil
//  cookieAcceptPolicy:NSHTTPCookieAcceptPolicyAlways];
- (instancetype)init;

@end

#endif  // IOS_WEB_DOWNLOAD_DOWNLOAD_SESSION_COOKIE_STORAGE_H_
