// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_CRW_WK_UI_HANDLER_H_
#define IOS_WEB_WEB_STATE_UI_CRW_WK_UI_HANDLER_H_

#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>

#import "ios/web/web_state/ui/crw_web_view_handler.h"

@protocol CRWWKUIHandlerDelegate;

// Object handling the WKUIDelegate callbacks for the WKWebView.
@interface CRWWKUIHandler : CRWWebViewHandler <WKUIDelegate>

// Delegate for the handler.
@property(nonatomic, weak) id<CRWWKUIHandlerDelegate> delegate;

@end

#endif  // IOS_WEB_WEB_STATE_UI_CRW_WK_UI_HANDLER_H_
