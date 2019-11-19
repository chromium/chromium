// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_DEPRECATED_CRW_CONTEXT_MENU_DELEGATE_H_
#define IOS_WEB_PUBLIC_DEPRECATED_CRW_CONTEXT_MENU_DELEGATE_H_

#import <WebKit/WebKit.h>

#import "ios/web/public/ui/context_menu_params.h"

// Implement this protocol to listen to the custom context menu trigger from
// WKWebView.
@protocol CRWContextMenuDelegate <NSObject>

// Called when the custom Context menu recognizer triggers on |webView| by a
// long press gesture. The system context menu will be suppressed.
- (void)webView:(WKWebView*)webView
    handleContextMenu:(const web::ContextMenuParams&)params;

@optional
// Called to execute JavaScript in |webView|. The |completionHandler| must be
// called with the result of executing |javaScript|. The JavaScript will be
// executed directly on |webView| if this method is not implemented.
- (void)webView:(WKWebView*)webView
    executeJavaScript:(NSString*)javaScript
    completionHandler:(void (^)(id, NSError*))completionHandler;
@end

#endif  // IOS_WEB_PUBLIC_DEPRECATED_CRW_CONTEXT_MENU_DELEGATE_H_
