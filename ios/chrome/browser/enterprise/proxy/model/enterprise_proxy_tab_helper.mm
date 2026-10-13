// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/proxy/model/enterprise_proxy_tab_helper.h"

#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"

EnterpriseProxyTabHelper::EnterpriseProxyTabHelper(web::WebState* web_state) {
  web_state_observation_.Observe(web_state);
}

EnterpriseProxyTabHelper::~EnterpriseProxyTabHelper() = default;

void EnterpriseProxyTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  // Same-document navigations never reach the network, so they cannot trigger
  // a proxy authentication challenge.
  if (navigation_context->IsSameDocument()) {
    return;
  }
  pending_navigation_id_ = navigation_context->GetNavigationId();
  pending_navigation_url_ = navigation_context->GetUrl();
}

void EnterpriseProxyTabHelper::DidRedirectNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (!IsPendingNavigation(navigation_context)) {
    return;
  }
  pending_navigation_url_ = navigation_context->GetUrl();
}

void EnterpriseProxyTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  // A newer navigation may have replaced the pending one before an older
  // navigation finishes, so only clear state for the tracked navigation.
  if (!IsPendingNavigation(navigation_context)) {
    return;
  }
  // TODO(crbug.com/543371697): Once `ProxyServiceController` attributes
  // challenges to navigations, check whether leftover disguised errors need to
  // be removed here via `EnterpriseProxyErrorService::RemoveDisguisedError()`.
  pending_navigation_id_.reset();
  pending_navigation_url_ = GURL();
}

bool EnterpriseProxyTabHelper::IsPendingNavigation(
    web::NavigationContext* navigation_context) const {
  return pending_navigation_id_ == navigation_context->GetNavigationId();
}
