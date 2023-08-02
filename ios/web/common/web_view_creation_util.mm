// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/common/web_view_creation_util.h"

#import "base/check.h"
#import "ios/web/common/user_agent.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#import "ios/web/web_state/web_view_internal_creation_util.h"

namespace web {

WKWebView* BuildWKWebView(CGRect frame, BrowserState* browser_state) {
  DCHECK(browser_state);

  WKWebViewConfigurationProvider& config_provider =
      WKWebViewConfigurationProvider::FromBrowserState(browser_state);
  return BuildWKWebView(frame, config_provider.GetWebViewConfiguration(),
                        browser_state, UserAgentType::MOBILE, nil);
}

WKWebView* BuildWKWebViewForQueries(BrowserState* browser_state) {
  DCHECK(browser_state);

  WKWebViewConfigurationProvider& config_provider =
      WKWebViewConfigurationProvider::FromBrowserState(browser_state);
  return BuildWKWebViewForQueries(config_provider.GetWebViewConfiguration(),
                                  browser_state);
}

}  // namespace web
