// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_MESSAGING_CRW_JS_WINDOW_ID_MANAGER_H_
#define IOS_WEB_JS_MESSAGING_CRW_JS_WINDOW_ID_MANAGER_H_

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

// Injects the JavaScript file window_id.js which sets __gCrWeb.windowId and
// manages the windowId for Page->Native->Page messages.
@interface CRWJSWindowIDManager : NSObject

// A unique window ID is assigned when the script is injected. Can not be null.
@property(nonatomic, copy, readonly) NSString* windowID;

- (instancetype)init NS_UNAVAILABLE;

// Initializes CRWJSWindowIDManager. |webView| will be used for script
// evaluation to inject window ID and can not be null.
- (instancetype)initWithWebView:(WKWebView*)webView NS_DESIGNATED_INITIALIZER;

// Injects windowId to a web page.
- (void)inject;

@end

#endif  // IOS_WEB_JS_MESSAGING_CRW_JS_WINDOW_ID_MANAGER_H_
