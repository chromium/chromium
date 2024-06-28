// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/browser_state_utils.h"

#import <WebKit/WebKit.h>

#import "ios/web/public/browser_state.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"

namespace web {

WKWebsiteDataStore* GetDataStoreForBrowserState(BrowserState* browser_state) {
  CHECK(browser_state);
  WKWebViewConfigurationProvider& provider =
      WKWebViewConfigurationProvider::FromBrowserState(browser_state);
  WKWebsiteDataStore* data_store =
      provider.GetWebViewConfiguration().websiteDataStore;
  CHECK(data_store);
  return data_store;
}

}  // namespace web
