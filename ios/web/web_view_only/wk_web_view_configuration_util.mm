// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_view_only/wk_web_view_configuration_util.h"

#import <WebKit/WebKit.h>

#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#import "ios/web/web_state/web_state_impl.h"

namespace web {

WKWebView* EnsureWebViewCreatedWithConfiguration(
    WebState* web_state,
    WKWebViewConfiguration* configuration) {
  WebStateImpl* impl = WebStateImpl::FromWebState(web_state);
  BrowserState* browser_state = impl->GetBrowserState();
  CRWWebController* web_controller = impl->GetWebController();

  WKWebViewConfigurationProvider& provider =
      WKWebViewConfigurationProvider::FromBrowserState(browser_state);
  provider.ResetWithWebViewConfiguration(configuration);

  // `web_controller` will get the `configuration` from the `provider` to create
  // the webView to return.
  return [web_controller ensureWebViewCreated];
}

}  // namespace web
