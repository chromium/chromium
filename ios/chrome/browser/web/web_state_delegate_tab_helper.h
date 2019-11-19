// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_WEB_STATE_DELEGATE_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_WEB_WEB_STATE_DELEGATE_TAB_HELPER_H_

#include "base/memory/weak_ptr.h"
#import "ios/chrome/browser/ui/dialogs/overlay_java_script_dialog_presenter.h"
#import "ios/web/public/web_state_delegate.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

class OverlayResponse;

// Tab helper that handles the WebStateDelegate implementation.
class WebStateDelegateTabHelper
    : public web::WebStateDelegate,
      public web::WebStateObserver,
      public web::WebStateUserData<WebStateDelegateTabHelper> {
 public:
  ~WebStateDelegateTabHelper() override;

  // TODO(crbug.com/986956): Move remaining WebStateDelegate implementations
  // into this tab helper.

  // web::WebStateDelegate:
  web::JavaScriptDialogPresenter* GetJavaScriptDialogPresenter(
      web::WebState* source) override;
  void OnAuthRequired(
      web::WebState* source,
      NSURLProtectionSpace* protection_space,
      NSURLCredential* proposed_credential,
      const web::WebStateDelegate::AuthCallback& callback) override;

 private:
  explicit WebStateDelegateTabHelper(web::WebState* web_state);
  friend class web::WebStateUserData<WebStateDelegateTabHelper>;
  WEB_STATE_USER_DATA_KEY_DECL();

  // WebStateObserver:
  void WebStateDestroyed(web::WebState* web_state) override;

  // Callback for HTTP authentication dialogs.
  void OnHTTPAuthOverlayFinished(web::WebStateDelegate::AuthCallback callback,
                                 OverlayResponse* response);

  OverlayJavaScriptDialogPresenter java_script_dialog_presenter_;
  base::WeakPtrFactory<WebStateDelegateTabHelper> weak_factory_;
};

#endif  // IOS_CHROME_BROWSER_WEB_WEB_STATE_DELEGATE_TAB_HELPER_H_
