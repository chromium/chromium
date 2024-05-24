// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_FAKE_AUTHENTICATION_SERVICE_DELEGATE_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_FAKE_AUTHENTICATION_SERVICE_DELEGATE_H_

#import "ios/chrome/browser/signin/model/authentication_service_delegate.h"

// Fake AuthenticationServiceDelegate used by FakeAuthenticationService.
class FakeAuthenticationServiceDelegate : public AuthenticationServiceDelegate {
 public:
  FakeAuthenticationServiceDelegate();

  FakeAuthenticationServiceDelegate& operator=(
      const FakeAuthenticationServiceDelegate&) = delete;

  ~FakeAuthenticationServiceDelegate() override;

  // AuthenticationServiceDelegate implementation.
  // Executes `completion` synchronously.
  void ClearBrowsingData(base::OnceClosure completion) override;
  void ClearBrowsingDataForSignedinPeriod(
      base::OnceClosure completion) override;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_FAKE_AUTHENTICATION_SERVICE_DELEGATE_H_
