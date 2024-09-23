// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_CRW_CONTEXT_MENU_CONTROLLER_H_
#define IOS_WEB_WEB_STATE_UI_CRW_CONTEXT_MENU_CONTROLLER_H_

#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>

namespace web {
class WebState;
}

// Controller for displaying system Context Menu when the user is long pressing
// on an element. This is working by adding an interaction to the whole web view
// and then only adding focus on the element being long pressed.
@interface CRWContextMenuController : NSObject

- (instancetype)initWithWebView:(WKWebView*)webView
                       webState:(web::WebState*)webState
                  containerView:(UIView*)containerView;

@end

#endif  // IOS_WEB_WEB_STATE_UI_CRW_CONTEXT_MENU_CONTROLLER_H_
