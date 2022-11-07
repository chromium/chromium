// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_AUTHENTICATION_SERVICE_DELEGATE_H_
#define IOS_CHROME_BROWSER_SIGNIN_AUTHENTICATION_SERVICE_DELEGATE_H_

#import "base/ios/block_types.h"

// Delegate for AuthenticationService.
class AuthenticationServiceDelegate {
 public:
  AuthenticationServiceDelegate() = default;

  AuthenticationServiceDelegate(const AuthenticationServiceDelegate&) = delete;
  AuthenticationServiceDelegate& operator=(
      const AuthenticationServiceDelegate&) = delete;

  virtual ~AuthenticationServiceDelegate() = default;

  // Invoked by AuthenticationService after the user has signed out. All the
  // local browsing data must be cleared out, then `completion` called.
  virtual void ClearBrowsingData(ProceduralBlock completion) = 0;

 private:
  friend class AuthenticationServiceTest;
  friend class FakeAuthenticationServiceDelegate;

  int clear_browsing_data_counter_ = 0;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_AUTHENTICATION_SERVICE_DELEGATE_H_
