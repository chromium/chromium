// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/common/web_view_creation_util.h"

#include "base/logging.h"
#include "ios/web/common/user_agent.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#import "ios/web/web_state/web_view_internal_creation_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

WKWebView* BuildWKWebView(CGRect frame, BrowserState* browser_state) {
  return BuildWKWebViewWithCustomContextMenu(frame, browser_state, nil);
}

WKWebView* BuildWKWebViewWithCustomContextMenu(
    CGRect frame,
    BrowserState* browser_state,
    id<CRWContextMenuDelegate> context_menu_delegate) {
  DCHECK(browser_state);

  WKWebViewConfigurationProvider& config_provider =
      WKWebViewConfigurationProvider::FromBrowserState(browser_state);
  return BuildWKWebView(frame, config_provider.GetWebViewConfiguration(),
                        browser_state, UserAgentType::MOBILE,
                        context_menu_delegate);
}

}  // namespace web
