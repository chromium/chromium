// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_PARENT_ACCESS_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_PARENT_ACCESS_TAB_HELPER_H_

#import "base/functional/callback.h"
#import "components/supervised_user/core/common/supervised_user_constants.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/web_state_user_data.h"

typedef base::OnceCallback<void(supervised_user::LocalApprovalResult result)>
    ParentAccessApprovalResultCallback;

@protocol ParentAccessCommands;

// Handles navigation within the parent access widget bottom sheet and
// processes the widget's local web approval results.
class ParentAccessTabHelper
    : public web::WebStatePolicyDecider,
      public web::WebStateUserData<ParentAccessTabHelper> {
 public:
  explicit ParentAccessTabHelper(web::WebState* web_state);
  ~ParentAccessTabHelper() override;

  ParentAccessTabHelper(const ParentAccessTabHelper&) = delete;
  ParentAccessTabHelper& operator=(const ParentAccessTabHelper&) = delete;

  // Sets the commands handler.
  void SetCommandsHandler(id<ParentAccessCommands> commands_handler);

  // Sets the completion callback handling the approval result.
  void SetApprovalResultCallback(ParentAccessApprovalResultCallback callback);

  // web::WebStatePolicyDecider
  void ShouldAllowRequest(
      NSURLRequest* request,
      web::WebStatePolicyDecider::RequestInfo request_info,
      web::WebStatePolicyDecider::PolicyDecisionCallback callback) override;

 private:
  // Handler to manage the parent access bottom sheet.
  __weak id<ParentAccessCommands> commands_handler_;
  ParentAccessApprovalResultCallback approval_result_callback_;

  friend class web::WebStateUserData<ParentAccessTabHelper>;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_PARENT_ACCESS_TAB_HELPER_H_
