// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/web_view_early_page_script_provider.h"

#import <Foundation/Foundation.h>

#include "ios/web/public/browser_state.h"
#include "ios/web/public/thread/web_thread.h"
#include "ios/web/web_state/ui/wk_web_view_configuration_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

namespace {
// A key used to associate a WebViewEarlyPageScriptProvider with a BrowserState.
const char kWebViewEarlyPageScriptProviderKeyName[] =
    "web_view_early_page_script_provider";

}  // namespace

// static
WebViewEarlyPageScriptProvider&
WebViewEarlyPageScriptProvider::FromBrowserState(
    web::BrowserState* _Nonnull browser_state) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  DCHECK(browser_state);
  if (!browser_state->GetUserData(kWebViewEarlyPageScriptProviderKeyName)) {
    browser_state->SetUserData(
        kWebViewEarlyPageScriptProviderKeyName,
        std::unique_ptr<WebViewEarlyPageScriptProvider>(
            new WebViewEarlyPageScriptProvider(browser_state)));
  }
  return *(static_cast<WebViewEarlyPageScriptProvider*>(
      browser_state->GetUserData(kWebViewEarlyPageScriptProviderKeyName)));
}

WebViewEarlyPageScriptProvider::~WebViewEarlyPageScriptProvider() = default;

void WebViewEarlyPageScriptProvider::SetScript(NSString* _Nonnull script) {
  script_ = [script copy];

  // Early page scripts must be explicitly updated after they change.
  web::WKWebViewConfigurationProvider& config_provider =
      web::WKWebViewConfigurationProvider::FromBrowserState(browser_state_);
  config_provider.UpdateScripts();
}

WebViewEarlyPageScriptProvider::WebViewEarlyPageScriptProvider(
    web::BrowserState* _Nonnull browser_state)
    : browser_state_(browser_state), script_([[NSString alloc] init]) {}

}  // namespace ios_web_view
