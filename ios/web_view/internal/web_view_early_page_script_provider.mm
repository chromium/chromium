// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/web_view_early_page_script_provider.h"

#import <Foundation/Foundation.h>

#include "ios/web/public/browser_state.h"
#include "ios/web/public/thread/web_thread.h"
#include "ios/web/web_state/ui/wk_web_view_configuration_provider.h"

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

void WebViewEarlyPageScriptProvider::SetScripts(
    NSString* _Nonnull all_frames_script,
    NSString* _Nonnull main_frame_script) {
  all_frames_script_ = [all_frames_script copy];
  main_frame_script_ = [main_frame_script copy];

  // Early page scripts must be explicitly updated after they change.
  web::WKWebViewConfigurationProvider& config_provider =
      web::WKWebViewConfigurationProvider::FromBrowserState(browser_state_);
  config_provider.UpdateScripts();
}

WebViewEarlyPageScriptProvider::WebViewEarlyPageScriptProvider(
    web::BrowserState* _Nonnull browser_state)
    : browser_state_(browser_state),
      all_frames_script_([[NSString alloc] init]),
      main_frame_script_([[NSString alloc] init]) {}

}  // namespace ios_web_view
