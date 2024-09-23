// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/safe_browsing/web_view_safe_browsing_client.h"

#import "base/check.h"
#import "base/memory/weak_ptr.h"
#import "ios/web/public/web_state.h"
#import "ios/web_view/internal/app/application_context.h"

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

safe_browsing::HashRealTimeService*
WebViewSafeBrowsingClient::GetHashRealTimeService() {
  // ios/web_view does not support hash-real-time lookups.
  return nullptr;
}

variations::VariationsService*
WebViewSafeBrowsingClient::GetVariationsService() {
  // ios/web_view does not support variations.
  return nullptr;
}

bool WebViewSafeBrowsingClient::ShouldBlockUnsafeResource(
    const security_interstitials::UnsafeResource& resource) const {
  return false;
}

bool WebViewSafeBrowsingClient::OnMainFrameUrlQueryCancellationDecided(
    web::WebState* web_state,
    const GURL& url) {
  // ios/web_view does not support OnMainFrameUrlQueryCancellationDecided.
  return true;
}
