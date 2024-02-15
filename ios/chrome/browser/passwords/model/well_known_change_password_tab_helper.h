// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_WELL_KNOWN_CHANGE_PASSWORD_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_WELL_KNOWN_CHANGE_PASSWORD_TAB_HELPER_H_

#import "base/memory/raw_ptr.h"
#include "components/password_manager/core/browser/well_known_change_password/well_known_change_password_state.h"
#include "components/password_manager/core/browser/well_known_change_password/well_known_change_password_util.h"
#include "ios/web/public/navigation/web_state_policy_decider.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

namespace affiliations {
class AffiliationService;
}  // namespace affiliations

// TODO(b/324553078): Move out of namespace.
namespace password_manager {

// This TabHelper checks whether a site supports the .well-known/change-password
// url. To check whether a site supports the change-password url the TabHelper
// also request a .well-known path that is defined to return a 404. When that
// one returns 404 and the change password path 2XX we assume the site supports
// the change-password url. If the site does not support the change password
// url, the user gets redirected to the base path '/'. If the sites supports the
// standard, the request is allowed and the navigation is not changed.
// The TabHelper only intercepts the navigation if .well-known/change-password
// is opened in a new tab.
class WellKnownChangePasswordTabHelper
    : public password_manager::WellKnownChangePasswordStateDelegate,
      public web::WebStateObserver,
      public web::WebStatePolicyDecider,
      public web::WebStateUserData<WellKnownChangePasswordTabHelper> {
 public:
  ~WellKnownChangePasswordTabHelper() override;
  // web::WebStatePolicyDecider:
  void ShouldAllowRequest(
      NSURLRequest* request,
      web::WebStatePolicyDecider::RequestInfo request_info,
      web::WebStatePolicyDecider::PolicyDecisionCallback callback) override;
  void ShouldAllowResponse(
      NSURLResponse* response,
      web::WebStatePolicyDecider::ResponseInfo response_info,
      web::WebStatePolicyDecider::PolicyDecisionCallback callback) override;
  void WebStateDestroyed() override;

  // web::WebStateObserver:
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void RenderProcessGone(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  friend class web::WebStateUserData<WellKnownChangePasswordTabHelper>;

  // Describes the progress and state of the .well-known processing.
  enum ProcessingState {
    // The TabHelper only checks the first request. This state signals if the
    // current request is the first.
    kWaitingForFirstRequest = 0,
    // When the first request is for .well-known/change-password, the TabHelper
    // is waiting for the response.
    kWaitingForResponse,
    // When the Response arrived the TabHelper waits for the
    // well_known_change_password_state_ to notify is the .well-known is
    // supported.
    kResponesReceived,
    // When the first request is not to .well-known/change-password, or another
    // navigation is started the TabHelper stops any custom processing.
    kInactive,
  };
  explicit WellKnownChangePasswordTabHelper(web::WebState* web_state);
  // WellKnownChangePasswordStateDelegate:
  void OnProcessingFinished(bool is_supported) override;
  // Redirects to a given URL in the same tab.
  void Redirect(const GURL& url);
  // Records the given UKM metric.
  void RecordMetric(WellKnownChangePasswordResult result);

  raw_ptr<web::WebState> web_state_ = nullptr;
  ProcessingState processing_state_ = kWaitingForFirstRequest;
  // Stores the request_url if the first navigation was to
  // .well-known/change-password. It is later used to redirect to the origin.
  GURL request_url_;
  // Stored callback from ShouldAllowResponse when the response is from
  // .well-known/change-password.
  web::WebStatePolicyDecider::PolicyDecisionCallback response_policy_callback_;
  password_manager::WellKnownChangePasswordState
      well_known_change_password_state_{this};
  raw_ptr<affiliations::AffiliationService> affiliation_service_ = nullptr;
  WEB_STATE_USER_DATA_KEY_DECL();
};

}  // namespace password_manager

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_WELL_KNOWN_CHANGE_PASSWORD_TAB_HELPER_H_
