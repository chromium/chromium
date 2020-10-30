// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/test_web_state_delegate.h"


#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

TestOpenURLRequest::TestOpenURLRequest()
    : params(GURL(),
             Referrer(),
             WindowOpenDisposition::UNKNOWN,
             ui::PAGE_TRANSITION_LINK,
             false) {}

TestOpenURLRequest::~TestOpenURLRequest() = default;

TestOpenURLRequest::TestOpenURLRequest(const TestOpenURLRequest&) = default;

TestRepostFormRequest::TestRepostFormRequest() = default;

TestRepostFormRequest::~TestRepostFormRequest() = default;

TestAuthenticationRequest::TestAuthenticationRequest() {}

TestAuthenticationRequest::~TestAuthenticationRequest() = default;

TestAuthenticationRequest::TestAuthenticationRequest(
    TestAuthenticationRequest&&) = default;

TestWebStateDelegate::TestWebStateDelegate() {}

TestWebStateDelegate::~TestWebStateDelegate() = default;

WebState* TestWebStateDelegate::CreateNewWebState(WebState* source,
                                                  const GURL& url,
                                                  const GURL& opener_url,
                                                  bool initiated_by_user) {
  last_create_new_web_state_request_ =
      std::make_unique<TestCreateNewWebStateRequest>();
  last_create_new_web_state_request_->web_state = source;
  last_create_new_web_state_request_->url = url;
  last_create_new_web_state_request_->opener_url = opener_url;
  last_create_new_web_state_request_->initiated_by_user = initiated_by_user;

  if (!initiated_by_user &&
      allowed_popups_.find(opener_url) == allowed_popups_.end()) {
    popups_.push_back(TestPopup(url, opener_url));
    return nullptr;
  }

  web::WebState::CreateParams params(source->GetBrowserState());
  params.created_with_opener = true;
  std::unique_ptr<web::WebState> child = web::WebState::Create(params);

  child_windows_.push_back(std::move(child));
  return child_windows_.back().get();
}

void TestWebStateDelegate::CloseWebState(WebState* source) {
  last_close_web_state_request_ = std::make_unique<TestCloseWebStateRequest>();
  last_close_web_state_request_->web_state = source;

  // Remove WebState from |child_windows_|.
  for (size_t i = 0; i < child_windows_.size(); i++) {
    if (child_windows_[i].get() == source) {
      closed_child_windows_.push_back(std::move(child_windows_[i]));
      child_windows_.erase(child_windows_.begin() + i);
      break;
    }
  }
}

WebState* TestWebStateDelegate::OpenURLFromWebState(
    WebState* web_state,
    const WebState::OpenURLParams& params) {
  last_open_url_request_ = std::make_unique<TestOpenURLRequest>();
  last_open_url_request_->web_state = web_state;
  last_open_url_request_->params = params;
  return nullptr;
}

JavaScriptDialogPresenter* TestWebStateDelegate::GetJavaScriptDialogPresenter(
    WebState*) {
  get_java_script_dialog_presenter_called_ = true;
  return &java_script_dialog_presenter_;
}

void TestWebStateDelegate::HandleContextMenu(WebState*,
                                             const ContextMenuParams&) {
  handle_context_menu_called_ = true;
}

void TestWebStateDelegate::ShowRepostFormWarningDialog(
    WebState* source,
    base::OnceCallback<void(bool)> callback) {
  last_repost_form_request_ = std::make_unique<TestRepostFormRequest>();
  last_repost_form_request_->web_state = source;
  last_repost_form_request_->callback = std::move(callback);
}

TestJavaScriptDialogPresenter*
TestWebStateDelegate::GetTestJavaScriptDialogPresenter() {
  return &java_script_dialog_presenter_;
}

void TestWebStateDelegate::OnAuthRequired(
    WebState* source,
    NSURLProtectionSpace* protection_space,
    NSURLCredential* credential,
    AuthCallback callback) {
  last_authentication_request_ = std::make_unique<TestAuthenticationRequest>();
  last_authentication_request_->web_state = source;
  last_authentication_request_->protection_space = protection_space;
  last_authentication_request_->credential = credential;
  last_authentication_request_->auth_callback = std::move(callback);
}

bool TestWebStateDelegate::ShouldPreviewLink(WebState* source,
                                             const GURL& link_url) {
  last_link_url_ = link_url;
  return should_preview_link_;
}

UIViewController* TestWebStateDelegate::GetPreviewingViewController(
    WebState* source,
    const GURL& link_url) {
  last_link_url_ = link_url;
  return previewing_view_controller_;
}

void TestWebStateDelegate::CommitPreviewingViewController(
    WebState* source,
    UIViewController* previewing_view_controller) {
  last_previewing_view_controller_ = previewing_view_controller;
}

}  // namespace web
