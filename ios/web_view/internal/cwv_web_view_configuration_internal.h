// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_CWV_WEB_VIEW_CONFIGURATION_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_CWV_WEB_VIEW_CONFIGURATION_INTERNAL_H_

#import "ios/web_view/public/cwv_web_view_configuration.h"

#include <memory>

NS_ASSUME_NONNULL_BEGIN

namespace ios_web_view {
class WebViewBrowserState;
}  // namespace ios_web_view

@class CWVWebView;

@interface CWVWebViewConfiguration ()

// The browser state associated with this configuration.
@property(nonatomic, readonly) ios_web_view::WebViewBrowserState* browserState;

// Calls |shutDown| on the singletons returned by |defaultConfiguration| and
// |incognitoConfiguration|.
+ (void)shutDown;

// Initializes with |browserState| that this instance is to be associated with.
- (instancetype)initWithBrowserState:
    (std::unique_ptr<ios_web_view::WebViewBrowserState>)browserState
    NS_DESIGNATED_INITIALIZER;

// Registers a |webView| so that this class can call |shutDown| on it later on.
// Only weak references are held, so no need for de-register method.
- (void)registerWebView:(CWVWebView*)webView;

// If this instance is going to outlive the globals created in
// ios_web_view::InitializeGlobalState, this method must be called to ensure the
// correct tear down order.
- (void)shutDown;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_INTERNAL_CWV_WEB_VIEW_CONFIGURATION_INTERNAL_H_
