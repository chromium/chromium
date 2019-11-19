// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_CRW_WK_UI_HANDLER_H_
#define IOS_WEB_WEB_STATE_UI_CRW_WK_UI_HANDLER_H_

#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>

@class CRWContextMenuController;
@protocol CRWWKUIHandlerDelegate;

// Object handling the WKUIDelegate callbacks for the WKWebView.
@interface CRWWKUIHandler : NSObject <WKUIDelegate>

// Delegate for the handler.
@property(nonatomic, weak) id<CRWWKUIHandlerDelegate> delegate;

// Context menu controller, to be set when the WebView is created.
@property(nonatomic, strong) CRWContextMenuController* contextMenuController;

// Closes the UI Handler.
- (void)close;

@end

#endif  // IOS_WEB_WEB_STATE_UI_CRW_WK_UI_HANDLER_H_
