// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_PARENT_ACCESS_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_PARENT_ACCESS_TAB_HELPER_H_

#include <optional>

#import "base/functional/callback.h"
#import "components/supervised_user/core/common/supervised_user_constants.h"
#import "ios/chrome/browser/supervised_user/model/parent_access_tab_helper_delegate.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/web_state_user_data.h"

typedef base::OnceCallback<void(
    supervised_user::LocalApprovalResult result,
    std::optional<supervised_user::LocalWebApprovalErrorType>)>
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

  // Sets the delegate.
  void SetDelegate(id<ParentAccessTabHelperDelegate> delegate);

  // web::WebStatePolicyDecider
  void ShouldAllowRequest(
      NSURLRequest* request,
      web::WebStatePolicyDecider::RequestInfo request_info,
      web::WebStatePolicyDecider::PolicyDecisionCallback callback) override;

 private:
  // Delegate to manage the parent access bottom sheet.
  id<ParentAccessTabHelperDelegate> delegate_;

  friend class web::WebStateUserData<ParentAccessTabHelper>;
};

#endif  // IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_PARENT_ACCESS_TAB_HELPER_H_
