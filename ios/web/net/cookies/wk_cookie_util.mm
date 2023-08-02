// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/net/cookies/wk_cookie_util.h"

#import <WebKit/WebKit.h>

#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"

namespace web {

WKHTTPCookieStore* WKCookieStoreForBrowserState(BrowserState* browser_state) {
  WKWebViewConfigurationProvider& config_provider =
      WKWebViewConfigurationProvider::FromBrowserState(browser_state);
  WKWebViewConfiguration* config = config_provider.GetWebViewConfiguration();
  return config.websiteDataStore.httpCookieStore;
}

}  // namespace web
