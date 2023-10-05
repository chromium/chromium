// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_URL_FILTER_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_URL_FILTER_TAB_HELPER_H_

#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/web_state_user_data.h"

// Handles filtered URL requests for supervised users.
class SupervisedUserURLFilterTabHelper
    : public web::WebStatePolicyDecider,
      public web::WebStateUserData<SupervisedUserURLFilterTabHelper> {
 public:
  explicit SupervisedUserURLFilterTabHelper(web::WebState* web_state);
  ~SupervisedUserURLFilterTabHelper() override;

  // web::WebStatePolicyDecider
  void ShouldAllowRequest(
      NSURLRequest* request,
      web::WebStatePolicyDecider::RequestInfo request_info,
      web::WebStatePolicyDecider::PolicyDecisionCallback callback) override;

 private:
  friend class web::WebStateUserData<SupervisedUserURLFilterTabHelper>;

  SupervisedUserURLFilterTabHelper(const SupervisedUserURLFilterTabHelper&) =
      delete;
  SupervisedUserURLFilterTabHelper& operator=(
      const SupervisedUserURLFilterTabHelper&) = delete;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_URL_FILTER_TAB_HELPER_H_
