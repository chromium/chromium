// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/browsing_data/system_cookie_store_util.h"

#import <WebKit/WebKit.h>

#import "ios/net/cookies/ns_http_system_cookie_store.h"
#include "ios/net/cookies/system_cookie_store.h"
#import "ios/web/net/cookies/wk_cookie_util.h"
#import "ios/web/net/cookies/wk_http_system_cookie_store.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

std::unique_ptr<net::SystemCookieStore> CreateSystemCookieStore(
    BrowserState* browser_state) {
    // Using WKHTTPCookieStore guarantee that cookies are always in sync and
    // allows SystemCookieStore to handle cookies for OffTheRecord browser.
    WKWebViewConfigurationProvider& config_provider =
        WKWebViewConfigurationProvider::FromBrowserState(browser_state);
    return std::make_unique<web::WKHTTPSystemCookieStore>(&config_provider);
}

}  // namespace web
