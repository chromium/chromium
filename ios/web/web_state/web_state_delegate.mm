// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

void WebStateDelegate::HandleContextMenu(WebState*, const ContextMenuParams&) {}

void WebStateDelegate::ShowRepostFormWarningDialog(
    WebState*,
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(true);
}

JavaScriptDialogPresenter* WebStateDelegate::GetJavaScriptDialogPresenter(
    WebState*) {
  return nullptr;
}

void WebStateDelegate::OnAuthRequired(WebState* source,
                                      NSURLProtectionSpace* protection_space,
                                      NSURLCredential* proposed_credential,
                                      const AuthCallback& callback) {
  callback.Run(nil, nil);
}

bool WebStateDelegate::ShouldPreviewLink(WebState* source,
                                         const GURL& link_url) {
  return false;
}

UIViewController* WebStateDelegate::GetPreviewingViewController(
    WebState* source,
    const GURL& link_url) {
  return nullptr;
}

void WebStateDelegate::CommitPreviewingViewController(
    WebState* source,
    UIViewController* previewing_view_controller) {}

void WebStateDelegate::Attach(WebState* source) {
  DCHECK(attached_states_.find(source) == attached_states_.end());
  attached_states_.insert(source);
}

void WebStateDelegate::Detach(WebState* source) {
  DCHECK(attached_states_.find(source) != attached_states_.end());
  attached_states_.erase(source);
}

}  // web
