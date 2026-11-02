// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_ENTERPRISE_PROXY_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_ENTERPRISE_PROXY_TAB_HELPER_H_

#import <cstdint>
#import <optional>

#import "base/memory/raw_ptr.h"
#import "base/memory/raw_ref.h"
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
// it also drops the disguised error recorded for it, if left unused, as it does
// for the pending navigation when the helper is destroyed.
// The re-sign-in requests of the Secure Gateway interstitial are forwarded to
// its delegate.
class EnterpriseProxyTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<EnterpriseProxyTabHelper> {
 public:
  // Handles the Secure Gateway re-sign-in requests.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called when the user asks to sign in again from the authentication
    // interstitial displayed in `web_state`.
    virtual void SignIn(web::WebState* web_state) = 0;
  };

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

  // Returns the delegate handling the re-sign-in requests, or `nullptr` if
  // there is none.
  Delegate* delegate() const { return delegate_; }

  // Sets the `delegate` handling the re-sign-in requests. `delegate` isn't
  // owned and must be reset to `nullptr` before it is destroyed.
  void SetDelegate(Delegate* delegate);

  // Forwards a re-sign-in request to the delegate. Does nothing if there is no
  // delegate.
  void SignIn();

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

  // The `WebState` this tab helper is attached to. A `const raw_ref` because it
  // is never null and never changes, and it outlives this tab helper: as
  // `WebState` user data, the tab helper is destroyed by `~WebStateImpl()`
  // while the `WebState` is still valid.
  const raw_ref<web::WebState> web_state_;
  // Service that holds the disguised errors recorded for navigations.
  const raw_ref<enterprise_net::EnterpriseProxyErrorService> error_service_;
  // Handles the re-sign-in requests. Not owned.
  raw_ptr<Delegate> delegate_ = nullptr;
  std::optional<int64_t> pending_navigation_id_;
  GURL pending_navigation_url_;

  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_ENTERPRISE_PROXY_TAB_HELPER_H_
