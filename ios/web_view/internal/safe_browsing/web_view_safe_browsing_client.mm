// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/safe_browsing/web_view_safe_browsing_client.h"

#import "base/check.h"
#import "base/memory/weak_ptr.h"
#import "ios/web/public/web_state.h"
#import "ios/web_view/internal/app/application_context.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

WebViewSafeBrowsingClient::WebViewSafeBrowsingClient() = default;

WebViewSafeBrowsingClient::~WebViewSafeBrowsingClient() = default;

base::WeakPtr<SafeBrowsingClient> WebViewSafeBrowsingClient::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

SafeBrowsingService* WebViewSafeBrowsingClient::GetSafeBrowsingService() {
  return ios_web_view::ApplicationContext::GetInstance()
      ->GetSafeBrowsingService();
}

safe_browsing::RealTimeUrlLookupService*
WebViewSafeBrowsingClient::GetRealTimeUrlLookupService() {
  // ios/web_view does not support real time lookups, for now.
  return nullptr;
}

bool WebViewSafeBrowsingClient::ShouldBlockUnsafeResource(
    const security_interstitials::UnsafeResource& resource) const {
  return false;
}

void WebViewSafeBrowsingClient::OnMainFrameUrlQueryCancellationDecided(
    web::WebState* web_state,
    const GURL& url) {
  // No op.
}

bool WebViewSafeBrowsingClient::OnSubFrameUrlQueryCancellationDecided(
    web::WebState* web_state,
    const GURL& url) {
  return true;
}
