// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_AUTHENTICATION_SERVICE_DELEGATE_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_AUTHENTICATION_SERVICE_DELEGATE_H_

// Delegate for AuthenticationService.
class AuthenticationServiceDelegate {
 public:
  AuthenticationServiceDelegate() = default;

  AuthenticationServiceDelegate(const AuthenticationServiceDelegate&) = delete;
  AuthenticationServiceDelegate& operator=(
      const AuthenticationServiceDelegate&) = delete;

  virtual ~AuthenticationServiceDelegate() = default;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_AUTHENTICATION_SERVICE_DELEGATE_H_
