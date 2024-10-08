// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state_delegate.h"

#import "base/containers/contains.h"

namespace web {

WebStateDelegate::WebStateDelegate() {}

WebStateDelegate::~WebStateDelegate() {
  while (!attached_states_.empty()) {
    WebState* web_state = *attached_states_.begin();
    web_state->SetDelegate(nullptr);
  }
  DCHECK(attached_states_.empty());
}

WebState* WebStateDelegate::CreateNewWebState(WebState* source,
                                              const GURL& url,
                                              const GURL& opener_url,
                                              bool initiated_by_user) {
  return nullptr;
}

void WebStateDelegate::CloseWebState(WebState* source) {}

WebState* WebStateDelegate::OpenURLFromWebState(
    WebState*,
    const WebState::OpenURLParams&) {
  return nullptr;
}

void WebStateDelegate::ShowRepostFormWarningDialog(
    WebState*,
    FormWarningType warning_type,
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(true);
}

JavaScriptDialogPresenter* WebStateDelegate::GetJavaScriptDialogPresenter(
    WebState*) {
  return nullptr;
}

void WebStateDelegate::HandlePermissionsDecisionRequest(
    WebState* source,
    NSArray<NSNumber*>* permissions,
    WebStatePermissionDecisionHandler handler) {
  handler(PermissionDecisionShowDefaultPrompt);
}

void WebStateDelegate::OnAuthRequired(WebState* source,
                                      NSURLProtectionSpace* protection_space,
                                      NSURLCredential* proposed_credential,
                                      AuthCallback callback) {
  std::move(callback).Run(nil, nil);
}

UIView* WebStateDelegate::GetWebViewContainer(WebState* source) {
  return nil;
}

void WebStateDelegate::Attach(WebState* source) {
  DCHECK(!base::Contains(attached_states_, source));
  attached_states_.insert(source);
}

void WebStateDelegate::Detach(WebState* source) {
  DCHECK(base::Contains(attached_states_, source));
  attached_states_.erase(source);
}

void WebStateDelegate::ContextMenuConfiguration(
    WebState* source,
    const ContextMenuParams& params,
    void (^completion_handler)(UIContextMenuConfiguration*)) {
  completion_handler(nil);
}

void WebStateDelegate::ContextMenuWillCommitWithAnimator(
    WebState* source,
    id<UIContextMenuInteractionCommitAnimating> animator) {}

id<CRWResponderInputView> WebStateDelegate::GetResponderInputView(
    WebState* source) {
  return nil;
}

void WebStateDelegate::OnNewWebViewCreated(WebState* source) {}

}  // web
