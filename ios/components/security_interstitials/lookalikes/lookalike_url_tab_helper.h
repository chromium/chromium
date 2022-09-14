// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_SECURITY_INTERSTITIALS_LOOKALIKES_LOOKALIKE_URL_TAB_HELPER_H_
#define IOS_COMPONENTS_SECURITY_INTERSTITIALS_LOOKALIKES_LOOKALIKE_URL_TAB_HELPER_H_

#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/web_state_user_data.h"

class LookalikeUrlTabHelper
    : public web::WebStatePolicyDecider,
      public web::WebStateUserData<LookalikeUrlTabHelper> {
 public:
  ~LookalikeUrlTabHelper() override;

  LookalikeUrlTabHelper(const LookalikeUrlTabHelper&) = delete;
  LookalikeUrlTabHelper& operator=(const LookalikeUrlTabHelper&) = delete;

 private:
  explicit LookalikeUrlTabHelper(web::WebState* web_state);
  friend class web::WebStateUserData<LookalikeUrlTabHelper>;
  WEB_STATE_USER_DATA_KEY_DECL();

  // web::WebStatePolicyDecider implementation
  void ShouldAllowResponse(
      NSURLResponse* response,
      web::WebStatePolicyDecider::ResponseInfo response_info,
      web::WebStatePolicyDecider::PolicyDecisionCallback callback) override;
};

#endif  // IOS_COMPONENTS_SECURITY_INTERSTITIALS_LOOKALIKES_LOOKALIKE_URL_TAB_HELPER_H_
