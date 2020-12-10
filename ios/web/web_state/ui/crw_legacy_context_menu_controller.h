// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_CRW_LEGACY_CONTEXT_MENU_CONTROLLER_H_
#define IOS_WEB_WEB_STATE_UI_CRW_LEGACY_CONTEXT_MENU_CONTROLLER_H_

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

namespace web {
class WebStateImpl;
}  // namespace web

// A controller that will recognise context menu gesture on |webView|. This
// controller will rely on a long press gesture recognizer and JavaScript to
// determine the element on which context menu is triggered.
// The trigger delay is slightly shorter that ths system's one.
// This is a Legacy interface as it is relying on ActionSheet whereas the new
// interface is using the iOS 13 ContextMenu.
@interface CRWLegacyContextMenuController : NSObject

// Installs the |CRWLegacyContextMenuController| on |webView|.
// - |webView| cannot be nil. |webView| is not retained and caller is
//   responsible for keeping it alive.
// - This class relies on the pre-injection of base.js in webView.
// - This class will perform gesture recognition and JavaScript on every touch
//   event on |webView| and can have performance impact.
- (instancetype)initWithWebView:(WKWebView*)webView
                       webState:(web::WebStateImpl*)webState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// By default, this controller "hooks" long touches to suppress the system
// default behavior (which shows the system context menu) and show its own
// context menu instead. This method disables the hook for the current on-going
// touch i.e., triggers the system default behavior.
- (void)allowSystemUIForCurrentGesture;

@end

#endif  // IOS_WEB_WEB_STATE_UI_CRW_LEGACY_CONTEXT_MENU_CONTROLLER_H_
