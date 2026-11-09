// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_FAKE_ENTERPRISE_PROXY_TAB_HELPER_DELEGATE_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_FAKE_ENTERPRISE_PROXY_TAB_HELPER_DELEGATE_H_

#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/enterprise/proxy/model/enterprise_proxy_tab_helper.h"

namespace web {
class WebState;
}  // namespace web

// Fake `EnterpriseProxyTabHelper::Delegate` that records re-sign-in requests.
class FakeEnterpriseProxyTabHelperDelegate final
    : public EnterpriseProxyTabHelper::Delegate {
 public:
  FakeEnterpriseProxyTabHelperDelegate();
  FakeEnterpriseProxyTabHelperDelegate(
      const FakeEnterpriseProxyTabHelperDelegate&) = delete;
  FakeEnterpriseProxyTabHelperDelegate& operator=(
      const FakeEnterpriseProxyTabHelperDelegate&) = delete;
  ~FakeEnterpriseProxyTabHelperDelegate() final;

  // `EnterpriseProxyTabHelper::Delegate`:
  void SignIn(web::WebState* web_state) final;

  // Returns the number of `SignIn()` calls received.
  int sign_in_count() const { return sign_in_count_; }

  // Returns the `WebState` passed to the most recent `SignIn()` call, or
  // `nullptr` if `SignIn()` has not been called, `nullptr` was passed, or the
  // `WebState` has since been destroyed.
  web::WebState* last_web_state() const { return last_web_state_.get(); }

 private:
  int sign_in_count_ = 0;
  base::WeakPtr<web::WebState> last_web_state_;
};

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_FAKE_ENTERPRISE_PROXY_TAB_HELPER_DELEGATE_H_
