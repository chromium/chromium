// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/features.h"
#import "ios/web/web_state/js/page_script_util.h"
#import "ios/web/web_state/ui/crw_wk_script_message_router.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

namespace {
// A key used to associate a WKWebViewConfigurationProvider with a BrowserState.
const char kWKWebViewConfigProviderKeyName[] = "wk_web_view_config_provider";

// Returns an autoreleased instance of WKUserScript to be added to
// configuration's userContentController.
WKUserScript* InternalGetDocumentStartScriptForMainFrame(
    BrowserState* browser_state) {
  return [[WKUserScript alloc]
        initWithSource:GetDocumentStartScriptForMainFrame(browser_state)
         injectionTime:WKUserScriptInjectionTimeAtDocumentStart
      forMainFrameOnly:YES];
}

// Returns an autoreleased instance of WKUserScript to be added to
// configuration's userContentController.
WKUserScript* InternalGetDocumentStartScriptForAllFrames(
    BrowserState* browser_state) {
  return [[WKUserScript alloc]
        initWithSource:GetDocumentStartScriptForAllFrames(browser_state)
         injectionTime:WKUserScriptInjectionTimeAtDocumentStart
      forMainFrameOnly:NO];
}

// Returns an autoreleased instance of WKUserScript to be added to
// configuration's userContentController.
WKUserScript* InternalGetDocumentEndScriptForAllFrames(
    BrowserState* browser_state) {
  return [[WKUserScript alloc]
        initWithSource:GetDocumentEndScriptForAllFrames(browser_state)
         injectionTime:WKUserScriptInjectionTimeAtDocumentEnd
      forMainFrameOnly:NO];
}

}  // namespace

// static
WKWebViewConfigurationProvider&
WKWebViewConfigurationProvider::FromBrowserState(BrowserState* browser_state) {
  DCHECK([NSThread isMainThread]);
  DCHECK(browser_state);
  if (!browser_state->GetUserData(kWKWebViewConfigProviderKeyName)) {
    browser_state->SetUserData(
        kWKWebViewConfigProviderKeyName,
        base::WrapUnique(new WKWebViewConfigurationProvider(browser_state)));
  }
  return *(static_cast<WKWebViewConfigurationProvider*>(
      browser_state->GetUserData(kWKWebViewConfigProviderKeyName)));
}

WKWebViewConfigurationProvider::WKWebViewConfigurationProvider(
    BrowserState* browser_state)
    : browser_state_(browser_state) {}

WKWebViewConfigurationProvider::~WKWebViewConfigurationProvider() {
}

WKWebViewConfiguration*
WKWebViewConfigurationProvider::GetWebViewConfiguration() {
  DCHECK([NSThread isMainThread]);
  if (!configuration_) {
    configuration_ = [[WKWebViewConfiguration alloc] init];
    if (browser_state_->IsOffTheRecord()) {
      [configuration_
          setWebsiteDataStore:[WKWebsiteDataStore nonPersistentDataStore]];
    }

    if (base::FeatureList::IsEnabled(
            web::features::kIgnoresViewportScaleLimits)) {
      [configuration_ setIgnoresViewportScaleLimits:YES];
    }

    [configuration_ setAllowsInlineMediaPlayback:YES];
    // setJavaScriptCanOpenWindowsAutomatically is required to support popups.
    [[configuration_ preferences] setJavaScriptCanOpenWindowsAutomatically:YES];
    // Main frame script depends upon scripts injected into all frames, so the
    // "AllFrames" scripts must be injected first.
    [[configuration_ userContentController]
        addUserScript:InternalGetDocumentStartScriptForAllFrames(
                          browser_state_)];
    [[configuration_ userContentController]
        addUserScript:InternalGetDocumentStartScriptForMainFrame(
                          browser_state_)];
    [[configuration_ userContentController]
        addUserScript:InternalGetDocumentEndScriptForAllFrames(browser_state_)];
  }
  // This is a shallow copy to prevent callers from changing the internals of
  // configuration.
  return [configuration_ copy];
}

CRWWKScriptMessageRouter*
WKWebViewConfigurationProvider::GetScriptMessageRouter() {
  DCHECK([NSThread isMainThread]);
  if (!router_) {
    WKUserContentController* userContentController =
        [GetWebViewConfiguration() userContentController];
    router_ = [[CRWWKScriptMessageRouter alloc]
        initWithUserContentController:userContentController];
  }
  return router_;
}

void WKWebViewConfigurationProvider::Purge() {
  DCHECK([NSThread isMainThread]);
  configuration_ = nil;
  router_ = nil;
}

}  // namespace web
