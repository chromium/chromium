// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_WEB_STATE_DELEGATE_H_
#define IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_WEB_STATE_DELEGATE_H_

#import "ios/web/public/web_state_delegate.h"

// Proxy class that forwards calls to original web state `WebStateDelegate`
// if the method is supported.
class ReaderModeWebStateDelegate : public web::WebStateDelegate {
 public:
  ReaderModeWebStateDelegate(web::WebState* original_web_state,
                             web::WebStateDelegate* web_state_delegate);
  ~ReaderModeWebStateDelegate() override;

  // WebStateDelegate overrides:
  web::WebState* CreateNewWebState(web::WebState* source,
                                   const GURL& url,
                                   const GURL& opener_url,
                                   bool initiated_by_user) override;
  void CloseWebState(web::WebState* source) override;
  web::WebState* OpenURLFromWebState(
      web::WebState*,
      const web::WebState::OpenURLParams&) override;
  web::JavaScriptDialogPresenter* GetJavaScriptDialogPresenter(
      web::WebState*) override;
  void ShowRepostFormWarningDialog(
      web::WebState* source,
      web::FormWarningType warning_type,
      base::OnceCallback<void(bool)> callback) override;
  void OnAuthRequired(web::WebState* source,
                      NSURLProtectionSpace* protection_space,
                      NSURLCredential* proposed_credential,
                      AuthCallback callback) override;
  void HandlePermissionsDecisionRequest(
      web::WebState* source,
      NSArray<NSNumber*>* permissions,
      web::WebStatePermissionDecisionHandler handler) override;
  void ContextMenuConfiguration(
      web::WebState* source,
      const web::ContextMenuParams& params,
      void (^completion_handler)(UIContextMenuConfiguration*)) override;
  void ContextMenuWillCommitWithAnimator(
      web::WebState* source,
      id<UIContextMenuInteractionCommitAnimating> animator) override;
  void ShouldAllowCopy(web::WebState* source,
                       base::OnceCallback<void(bool)> callback) override;

 private:
  raw_ptr<web::WebState> original_web_state_ = nullptr;
  raw_ptr<WebStateDelegate> web_state_delegate_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_WEB_STATE_DELEGATE_H_
