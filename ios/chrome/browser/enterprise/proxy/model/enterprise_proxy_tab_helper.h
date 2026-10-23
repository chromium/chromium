// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_ENTERPRISE_PROXY_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_ENTERPRISE_PROXY_TAB_HELPER_H_

#import <cstdint>
#import <optional>

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"
#import "url/gurl.h"

namespace enterprise_net {
class EnterpriseProxyErrorService;
}  // namespace enterprise_net

namespace web {
class NavigationContext;
class WebState;
}  // namespace web

// Per-WebState helper that tracks the pending cross-document main frame
// navigation for Enterprise Proxy authentication challenges. It records the
// navigation ID and URL when the navigation starts, follows server redirects,
// and drops both once that navigation finishes. When any navigation finishes,
// it also drops the enterprise proxy error recorded for it, if left unused, as
// it does for the pending navigation when the helper is destroyed.
class EnterpriseProxyTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<EnterpriseProxyTabHelper> {
 public:
  EnterpriseProxyTabHelper(const EnterpriseProxyTabHelper&) = delete;
  EnterpriseProxyTabHelper& operator=(const EnterpriseProxyTabHelper&) = delete;

  ~EnterpriseProxyTabHelper() override;

  // Returns the ID of the pending cross-document main frame navigation, or
  // `std::nullopt` if there is none.
  std::optional<int64_t> pending_navigation_id() const {
    return pending_navigation_id_;
  }

  // Returns the URL of the pending cross-document main frame navigation,
  // updated on server redirects. Empty if there is no pending navigation.
  const GURL& pending_navigation_url() const { return pending_navigation_url_; }

  // web::WebStateObserver:
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void DidRedirectNavigation(
      web::WebState* web_state,
      web::NavigationContext* navigation_context) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;

 private:
  friend class web::WebStateUserData<EnterpriseProxyTabHelper>;

  // `error_service` must not be null. It is a profile keyed service, so it
  // outlives the WebStates of its profile.
  EnterpriseProxyTabHelper(
      web::WebState* web_state,
      enterprise_net::EnterpriseProxyErrorService* error_service);

  // Returns whether `navigation_context` is the pending navigation.
  bool IsPendingNavigation(web::NavigationContext* navigation_context) const;

  // Drops errors recorded for finished navigations.
  raw_ptr<enterprise_net::EnterpriseProxyErrorService> error_service_ = nullptr;
  std::optional<int64_t> pending_navigation_id_;
  GURL pending_navigation_url_;

  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_ENTERPRISE_PROXY_TAB_HELPER_H_
