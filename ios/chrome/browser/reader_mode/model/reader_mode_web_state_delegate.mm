// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_web_state_delegate.h"

ReaderModeWebStateDelegate::ReaderModeWebStateDelegate(
    web::WebState* original_web_state,
    web::WebStateDelegate* web_state_delegate)
    : original_web_state_(original_web_state),
      web_state_delegate_(web_state_delegate) {}

ReaderModeWebStateDelegate::~ReaderModeWebStateDelegate() = default;

web::WebState* ReaderModeWebStateDelegate::CreateNewWebState(
    web::WebState* source,
    const GURL& url,
    const GURL& opener_url,
    bool initiated_by_user) {
  // Override the source forwarded to `CreateNewWebState` so the host WebState
  // will be considered as the source for the new WebState created as a result
  // of the navigation action.
  source = original_web_state_;
  return web_state_delegate_->CreateNewWebState(source, url, opener_url,
                                                initiated_by_user);
}

void ReaderModeWebStateDelegate::CloseWebState(web::WebState* source) {
  NOTREACHED();
}

web::WebState* ReaderModeWebStateDelegate::OpenURLFromWebState(
    web::WebState* web_state,
    const web::WebState::OpenURLParams& params) {
  NOTREACHED();
}

web::JavaScriptDialogPresenter*
ReaderModeWebStateDelegate::GetJavaScriptDialogPresenter(
    web::WebState* web_state) {
  return nullptr;
}

void ReaderModeWebStateDelegate::ShowRepostFormWarningDialog(
    web::WebState* source,
    web::FormWarningType warning_type,
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(false);
}

void ReaderModeWebStateDelegate::OnAuthRequired(
    web::WebState* source,
    NSURLProtectionSpace* protection_space,
    NSURLCredential* credential,
    AuthCallback callback) {
  std::move(callback).Run(nullptr, nullptr);
}

void ReaderModeWebStateDelegate::HandlePermissionsDecisionRequest(
    web::WebState* source,
    NSArray<NSNumber*>* permissions,
    web::WebStatePermissionDecisionHandler handler) {
  handler(web::PermissionDecisionShowDefaultPrompt);
}

void ReaderModeWebStateDelegate::ContextMenuConfiguration(
    web::WebState* source,
    const web::ContextMenuParams& params,
    void (^completion_handler)(UIContextMenuConfiguration*)) {
  return web_state_delegate_->ContextMenuConfiguration(source, params,
                                                       completion_handler);
}

void ReaderModeWebStateDelegate::ContextMenuWillCommitWithAnimator(
    web::WebState* source,
    id<UIContextMenuInteractionCommitAnimating> animator) {
  return web_state_delegate_->ContextMenuWillCommitWithAnimator(source,
                                                                animator);
}

void ReaderModeWebStateDelegate::ShouldAllowCopy(
    web::WebState* source,
    base::OnceCallback<void(bool)> callback) {
  web_state_delegate_->ShouldAllowCopy(source, std::move(callback));
}
