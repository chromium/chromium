// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/supervised_user_error_container.h"

#import <string>

#import "base/memory/ptr_util.h"
#import "components/supervised_user/core/browser/supervised_user_service.h"
#import "components/supervised_user/core/browser/supervised_user_url_filter.h"
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
// The method is invoked as a callback, so it is recommended to
// bind a weak pointer to the webstate, in case it has been invalidated.
void OnRequestUrlAccessRemote(base::WeakPtr<web::WebState> weak_web_state,
                              bool is_main_frame,
                              bool is_request_successful) {
  web::WebState* web_state = weak_web_state.get();
  if (!web_state) {
    return;
  }
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
    : supervised_user_service_(
          *SupervisedUserServiceFactory::GetForBrowserState(
              ChromeBrowserState::FromBrowserState(
                  web_state->GetBrowserState()))),
      web_state_(web_state) {
  CHECK(SupervisedUserServiceFactory::GetForBrowserState(
      ChromeBrowserState::FromBrowserState(web_state->GetBrowserState())));
  supervised_user_service_->AddObserver(this);
}

SupervisedUserErrorContainer::~SupervisedUserErrorContainer() {
  supervised_user_service_->RemoveObserver(this);
}

SupervisedUserErrorContainer::SupervisedUserErrorInfo::SupervisedUserErrorInfo(
    const GURL& request_url,
    bool is_main_frame,
    supervised_user::FilteringBehaviorReason filtering_behavior_reason) {
  request_url_ = request_url;
  is_main_frame_ = is_main_frame;
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

  interstitial_ = supervised_user::SupervisedUserInterstitial::Create(
      std::move(web_content_handler), supervised_user_service_.get(),
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
    RequestUrlAccessRemoteCallback callback =
        base::BindOnce(&OnRequestUrlAccessRemote, web_state_->GetWeakPtr(),
                       supervised_user_error_info_->is_main_frame());

    interstitial_->RequestUrlAccessRemote(
        base::BindOnce(&SupervisedUserErrorContainer::OnRequestCreated,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       interstitial_->url().host()));

  } else if (command ==
             supervised_user::SupervisedUserInterstitial::Commands::BACK) {
    interstitial_->GoBack();
  }
}

bool SupervisedUserErrorContainer::IsRemoteApprovalPendingForUrl(
    const GURL& url) {
  return base::Contains(requested_hosts_, url.host());
}

void SupervisedUserErrorContainer::OnURLFilterChanged() {
  // TODO(b/264669960): Trigger approved webpage re-load.
  // See `SupervisedUserNavigationObserver::OnURLFilterChanged()`.
  MaybeUpdatePendingApprovals();
}

void SupervisedUserErrorContainer::OnRequestCreated(
    RequestUrlAccessRemoteCallback callback,
    const std::string& host,
    bool successfully_created_request) {
  if (successfully_created_request) {
    requested_hosts_.insert(host);
  }
  std::move(callback).Run(successfully_created_request);
}

void SupervisedUserErrorContainer::MaybeUpdatePendingApprovals() {
  supervised_user::SupervisedUserURLFilter::FilteringBehavior
      filtering_behavior;
  supervised_user::SupervisedUserURLFilter* url_filter =
      supervised_user_service_->GetURLFilter();

  for (auto iter = requested_hosts_.begin(); iter != requested_hosts_.end();) {
    bool is_manual = url_filter->GetManualFilteringBehaviorForURL(
        GURL(*iter), &filtering_behavior);

    if (is_manual && filtering_behavior ==
                         supervised_user::SupervisedUserURLFilter::
                             FilteringBehavior::ALLOW) {
      iter = requested_hosts_.erase(iter);
    } else {
      iter++;
    }
  }
}
