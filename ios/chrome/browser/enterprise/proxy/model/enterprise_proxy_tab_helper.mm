// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/proxy/model/enterprise_proxy_tab_helper.h"

#import "base/check.h"
#import "components/enterprise/net/core/enterprise_proxy_error_service.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"

EnterpriseProxyTabHelper::EnterpriseProxyTabHelper(
    web::WebState* web_state,
    enterprise_net::EnterpriseProxyErrorService* error_service)
    : error_service_(error_service) {
  CHECK(error_service_);
  web_state_observation_.Observe(web_state);
}

EnterpriseProxyTabHelper::~EnterpriseProxyTabHelper() {
  // A navigation still pending when the WebState is destroyed never finishes,
  // so `DidFinishNavigation()` can't drop the error recorded for it.
  if (pending_navigation_id_.has_value()) {
    error_service_->RemoveDisguisedError(*pending_navigation_id_);
  }
}

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
  // Drops the error recorded for this navigation if nothing took it, e.g. the
  // navigation was stopped. A failed navigation finishes only after
  // `PrepareErrorPage()` ran, so an error page could already take its error.
  // Runs before the pending check so that replaced navigations are covered.
  error_service_->RemoveDisguisedError(navigation_context->GetNavigationId());

  // A newer navigation may have replaced the pending one before an older
  // navigation finishes, so only clear state for the tracked navigation.
  if (!IsPendingNavigation(navigation_context)) {
    return;
  }
  pending_navigation_id_.reset();
  pending_navigation_url_ = GURL();
}

bool EnterpriseProxyTabHelper::IsPendingNavigation(
    web::NavigationContext* navigation_context) const {
  return pending_navigation_id_ == navigation_context->GetNavigationId();
}
