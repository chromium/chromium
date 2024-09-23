// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/fake_web_state_delegate.h"

#import "base/containers/contains.h"

namespace web {

FakeOpenURLRequest::FakeOpenURLRequest()
    : params(GURL(),
             Referrer(),
             WindowOpenDisposition::UNKNOWN,
             ui::PAGE_TRANSITION_LINK,
             false) {}

FakeOpenURLRequest::~FakeOpenURLRequest() = default;

FakeOpenURLRequest::FakeOpenURLRequest(const FakeOpenURLRequest&) = default;

FakeRepostFormRequest::FakeRepostFormRequest() = default;

FakeRepostFormRequest::~FakeRepostFormRequest() = default;

FakeAuthenticationRequest::FakeAuthenticationRequest() {}

FakeAuthenticationRequest::~FakeAuthenticationRequest() = default;

FakeAuthenticationRequest::FakeAuthenticationRequest(
    FakeAuthenticationRequest&&) = default;

FakeWebStateDelegate::FakeWebStateDelegate() {}

FakeWebStateDelegate::~FakeWebStateDelegate() = default;

WebState* FakeWebStateDelegate::CreateNewWebState(WebState* source,
                                                  const GURL& url,
                                                  const GURL& opener_url,
                                                  bool initiated_by_user) {
  last_create_new_web_state_request_ =
      std::make_unique<FakeCreateNewWebStateRequest>();
  last_create_new_web_state_request_->web_state = source;
  last_create_new_web_state_request_->url = url;
  last_create_new_web_state_request_->opener_url = opener_url;
  last_create_new_web_state_request_->initiated_by_user = initiated_by_user;

  if (!initiated_by_user && !base::Contains(allowed_popups_, opener_url)) {
    popups_.push_back(FakePopup(url, opener_url));
    return nullptr;
  }

  web::WebState::CreateParams params(source->GetBrowserState());
  params.created_with_opener = true;
  std::unique_ptr<web::WebState> child = web::WebState::Create(params);

  child_windows_.push_back(std::move(child));
  return child_windows_.back().get();
}

void FakeWebStateDelegate::CloseWebState(WebState* source) {
  last_close_web_state_request_ = std::make_unique<FakeCloseWebStateRequest>();
  last_close_web_state_request_->web_state = source;

  // Remove WebState from `child_windows_`.
  for (size_t i = 0; i < child_windows_.size(); i++) {
    if (child_windows_[i].get() == source) {
      closed_child_windows_.push_back(std::move(child_windows_[i]));
      child_windows_.erase(child_windows_.begin() + i);
      break;
    }
  }
}

WebState* FakeWebStateDelegate::OpenURLFromWebState(
    WebState* web_state,
    const WebState::OpenURLParams& params) {
  last_open_url_request_ = std::make_unique<FakeOpenURLRequest>();
  last_open_url_request_->web_state = web_state;
  last_open_url_request_->params = params;
  return nullptr;
}

JavaScriptDialogPresenter* FakeWebStateDelegate::GetJavaScriptDialogPresenter(
    WebState*) {
  get_java_script_dialog_presenter_called_ = true;
  return &java_script_dialog_presenter_;
}

void FakeWebStateDelegate::ShowRepostFormWarningDialog(
    WebState* source,
    web::FormWarningType warningType,
    base::OnceCallback<void(bool)> callback) {
  // TODO(crbug.com/40941405): Handle warningType as well.
  last_repost_form_request_ = std::make_unique<FakeRepostFormRequest>();
  last_repost_form_request_->web_state = source;
  last_repost_form_request_->callback = std::move(callback);
}

FakeJavaScriptDialogPresenter*
FakeWebStateDelegate::GetFakeJavaScriptDialogPresenter() {
  return &java_script_dialog_presenter_;
}

void FakeWebStateDelegate::OnAuthRequired(
    WebState* source,
    NSURLProtectionSpace* protection_space,
    NSURLCredential* credential,
    AuthCallback callback) {
  last_authentication_request_ = std::make_unique<FakeAuthenticationRequest>();
  last_authentication_request_->web_state = source;
  last_authentication_request_->protection_space = protection_space;
  last_authentication_request_->credential = credential;
  last_authentication_request_->auth_callback = std::move(callback);
}

void FakeWebStateDelegate::HandlePermissionsDecisionRequest(
    WebState* source,
    NSArray<NSNumber*>* permissions,
    WebStatePermissionDecisionHandler handler) {
  last_requested_permissions_ = permissions;
  if (should_handle_permission_decision_) {
    handler(permission_decision_);
  }
}

}  // namespace web
