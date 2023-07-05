// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/supervised_user_error_container.h"

#import <string>

#import "base/memory/ptr_util.h"
#import "base/strings/stringprintf.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/supervised_user/ios_web_content_handler_impl.h"
#import "ios/chrome/browser/supervised_user/supervised_user_service_factory.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

WEB_STATE_USER_DATA_KEY_IMPL(SupervisedUserErrorContainer)

namespace {

const char* BoolToString(bool value) {
  return value ? "true" : "false";
}

// Updates the request status on the Interstitial upon making
// a permission request.
void RemoteApprovalRequestCallback(web::WebState* web_state,
                                   bool is_main_frame,
                                   bool is_request_successful) {
  CHECK(web_state);
  NSString* js_to_execute =
      [NSString stringWithFormat:@"setRequestStatus(%s, %s)",
                                 BoolToString(is_request_successful),
                                 BoolToString(is_main_frame)];
  // Trigger Intersitial JS method.
  web_state->ExecuteUserJavaScript(js_to_execute);
}
}  // namespace

SupervisedUserErrorContainer::SupervisedUserErrorContainer(
    web::WebState* web_state)
    : web_state_(web_state) {}

SupervisedUserErrorContainer::~SupervisedUserErrorContainer() = default;

SupervisedUserErrorContainer::SupervisedUserErrorInfo::SupervisedUserErrorInfo(
    const GURL& request_url,
    bool is_main_frame,
    bool is_already_requested,
    supervised_user::FilteringBehaviorReason filtering_behavior_reason) {
  request_url_ = request_url;
  is_main_frame_ = is_main_frame;
  is_already_requested_ = is_already_requested;
  filtering_behavior_reason_ = filtering_behavior_reason;
}
void SupervisedUserErrorContainer::SetSupervisedUserErrorInfo(
    std::unique_ptr<SupervisedUserErrorInfo> error_info) {
  supervised_user_error_info_ = std::move(error_info);
}

SupervisedUserErrorContainer::SupervisedUserErrorInfo&
SupervisedUserErrorContainer::GetSupervisedUserErrorInfo() {
  return *supervised_user_error_info_.get();
}

void SupervisedUserErrorContainer::CreateSupervisedUserInterstitial() {
  CHECK(supervised_user_error_info_);

  std::unique_ptr<IOSWebContentHandlerImpl> web_content_handler =
      std::make_unique<IOSWebContentHandlerImpl>(
          web_state_, supervised_user_error_info_->is_main_frame());

  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(web_state_->GetBrowserState());

  interstitial_ = supervised_user::SupervisedUserInterstitial::Create(
      std::move(web_content_handler),
      *SupervisedUserServiceFactory::GetForBrowserState(browser_state),
      supervised_user_error_info_->request_url(),
      // User name needed only for the local web approval flow, not applicable
      // for iOS.
      /*supervised_user_name=*/std::u16string(),
      supervised_user_error_info_->filtering_behavior_reason());
}

void SupervisedUserErrorContainer::HandleCommand(
    supervised_user::SupervisedUserInterstitial::Commands command) {
  CHECK(interstitial_);
  if (command == supervised_user::SupervisedUserInterstitial::Commands::
                     REMOTE_ACCESS_REQUEST) {
    base::OnceCallback<void(bool)> callback =
        base::BindOnce(&RemoteApprovalRequestCallback, web_state_,
                       supervised_user_error_info_->is_main_frame());
    interstitial_->RequestUrlAccessRemote(std::move(callback));
  } else if (command ==
             supervised_user::SupervisedUserInterstitial::Commands::BACK) {
    interstitial_->GoBack();
  }
}
