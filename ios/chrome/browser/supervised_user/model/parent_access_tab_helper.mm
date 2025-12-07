// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/model/parent_access_tab_helper.h"

#import "base/logging.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/supervised_user/core/browser/proto/parent_access_callback.pb.h"
#import "components/supervised_user/core/browser/supervised_user_utils.h"
#import "components/supervised_user/core/common/supervised_user_constants.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/parent_access_commands.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/supervised_user/model/parent_access_tab_helper_delegate.h"
#import "ios/web/public/web_state_user_data.h"
#import "net/base/apple/url_conversions.h"

using kids::platform::parentaccess::client::proto::ParentAccessCallback;

ParentAccessTabHelper::ParentAccessTabHelper(web::WebState* web_state)
    : web::WebStatePolicyDecider(web_state) {}

ParentAccessTabHelper::~ParentAccessTabHelper() = default;

void ParentAccessTabHelper::SetDelegate(
    id<ParentAccessTabHelperDelegate> delegate) {
  delegate_ = delegate;
}

void ParentAccessTabHelper::ShouldAllowRequest(
    NSURLRequest* request,
    web::WebStatePolicyDecider::RequestInfo request_info,
    web::WebStatePolicyDecider::PolicyDecisionCallback callback) {
  NSURL* url = request.URL;

  std::optional<std::string> encoded_callback =
      supervised_user::MaybeGetPacpResultFromUrl(net::GURLWithNSURL(url));

  // Allow the request to proceed if it does not include an encoded result.
  if (!encoded_callback.has_value()) {
    std::move(callback).Run(PolicyDecision::Allow());
    return;
  }

  if (encoded_callback.value().empty()) {
    // The `result` query param was empty.
    [delegate_
        hideParentAccessBottomSheetWithResult:supervised_user::
                                                  LocalApprovalResult::kError
                                    errorType:supervised_user::
                                                  LocalWebApprovalErrorType::
                                                      kPacpEmptyResponse];
    std::move(callback).Run(PolicyDecision::Allow());
    return;
  }

  auto parsed_result = supervised_user::ParentAccessCallbackParsedResult::
      ParseParentAccessCallbackResult(encoded_callback.value());

  if (parsed_result.GetError().has_value()) {
    supervised_user::LocalWebApprovalErrorType parseErrorType;
    switch (parsed_result.GetError().value()) {
      case supervised_user::ParentAccessWidgetError::kDecodingError:
        parseErrorType = supervised_user::LocalWebApprovalErrorType::
            kFailureToDecodePacpResponse;
        break;
      case supervised_user::ParentAccessWidgetError::kParsingError:
        parseErrorType = supervised_user::LocalWebApprovalErrorType::
            kFailureToParsePacpResponse;
        break;
      default:
        NOTREACHED();
    }
    [delegate_ hideParentAccessBottomSheetWithResult:
                   supervised_user::LocalApprovalResult::kError
                                           errorType:parseErrorType];
    std::move(callback).Run(PolicyDecision::Allow());
    return;
  }

  CHECK(parsed_result.GetCallback().has_value());
  ParentAccessCallback parent_access_callback =
      parsed_result.GetCallback().value();
  supervised_user::LocalApprovalResult result;
  std::optional<supervised_user::LocalWebApprovalErrorType> errorType;

  switch (parent_access_callback.callback_case()) {
    case ParentAccessCallback::CallbackCase::kOnParentVerified:
      result = supervised_user::LocalApprovalResult::kApproved;
      break;
      // TODO(crbug.com/384514294): Add support for the cancellation message
      // once PACP returns it for the approval flow.
    default:
      result = supervised_user::LocalApprovalResult::kError;
      errorType =
          supervised_user::LocalWebApprovalErrorType::kUnexpectedPacpResponse;
      break;
  }

  [delegate_ hideParentAccessBottomSheetWithResult:result errorType:errorType];
  std::move(callback).Run(PolicyDecision::Allow());
}
