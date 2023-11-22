// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MAIN_MODEL_BROWSER_WEB_STATE_LIST_DELEGATE_H_
#define IOS_CHROME_BROWSER_MAIN_MODEL_BROWSER_WEB_STATE_LIST_DELEGATE_H_

#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_delegate.h"

// WebStateList delegate used by Browser implementation.
class BrowserWebStateListDelegate : public WebStateListDelegate {
 public:
  // Policy controlling what to do when a WebState is inserted in the
  // WebStateList.
  enum class InsertionPolicy {
    kDoNothing,
    kAttachTabHelpers,
  };

  // Policy Controlling what to do when a WebState is activated.
  enum class ActivationPolicy {
    kDoNothing,
    kForceRealization,
  };

  // Creates a BrowserWebStateListDelegate with default policies.
  BrowserWebStateListDelegate();

  // Creates a BrowserWebStateListDelegate with specific policies.
  BrowserWebStateListDelegate(InsertionPolicy insertion_policy,
                              ActivationPolicy activation_policy);

  ~BrowserWebStateListDelegate() override;

  // WebStateListDelegate implementation.
  void WillAddWebState(web::WebState* web_state) override;
  void WillActivateWebState(web::WebState* web_state) override;

 private:
  // Controls what to do when a WebState is inserted.
  const InsertionPolicy insertion_policy_;

  // Controls what to do when a WebState is marked as active.
  const ActivationPolicy activation_policy_;
};

#endif  // IOS_CHROME_BROWSER_MAIN_MODEL_BROWSER_WEB_STATE_LIST_DELEGATE_H_
