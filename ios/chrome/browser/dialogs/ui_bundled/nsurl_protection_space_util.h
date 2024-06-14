// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DIALOGS_UI_BUNDLED_NSURL_PROTECTION_SPACE_UTIL_H_
#define IOS_CHROME_BROWSER_DIALOGS_UI_BUNDLED_NSURL_PROTECTION_SPACE_UTIL_H_

#import <Foundation/Foundation.h>

class GURL;

namespace nsurlprotectionspace_util {

// Information describing dialog requester used as dialog subtitle.
NSString* MessageForHTTPAuth(NSURLProtectionSpace* protectionSpace);

// Returns YES if dialog can be shown for set `protectionSpace`.
BOOL CanShow(NSURLProtectionSpace* protectionSpace);

// String represending authentication requester (origin URL or hostname).
NSString* RequesterIdentity(NSURLProtectionSpace* protectionSpace);

// URL origin of the dialog requester.
GURL RequesterOrigin(NSURLProtectionSpace* protectionSpace);

}  // namespace nsurlprotectionspace_util

#endif  // IOS_CHROME_BROWSER_DIALOGS_UI_BUNDLED_NSURL_PROTECTION_SPACE_UTIL_H_
