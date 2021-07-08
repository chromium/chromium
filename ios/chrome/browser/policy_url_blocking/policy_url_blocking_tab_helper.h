// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_URL_BLOCKING_POLICY_URL_BLOCKING_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_POLICY_URL_BLOCKING_POLICY_URL_BLOCKING_TAB_HELPER_H_

#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/web_state_user_data.h"

// A tab helper which handles blocking requests based on enterprise policy.
class PolicyUrlBlockingTabHelper
    : public web::WebStatePolicyDecider,
      public web::WebStateUserData<PolicyUrlBlockingTabHelper> {
 public:
  explicit PolicyUrlBlockingTabHelper(web::WebState* web_state);
  ~PolicyUrlBlockingTabHelper() override;

  // web::WebStatePolicyDecider
  web::WebStatePolicyDecider::PolicyDecision ShouldAllowRequest(
      NSURLRequest* request,
      const web::WebStatePolicyDecider::RequestInfo& request_info) override;

 private:
  friend class web::WebStateUserData<PolicyUrlBlockingTabHelper>;

  PolicyUrlBlockingTabHelper(const PolicyUrlBlockingTabHelper&) = delete;
  PolicyUrlBlockingTabHelper& operator=(const PolicyUrlBlockingTabHelper&) =
      delete;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_POLICY_URL_BLOCKING_POLICY_URL_BLOCKING_TAB_HELPER_H_
