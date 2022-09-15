// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/oauth_token_getter_impl.h"

namespace remoting {

// OAuthAuthorizationCredentials implementation.

OAuthTokenGetter::OAuthAuthorizationCredentials::OAuthAuthorizationCredentials(
    const std::string& login,
    const std::string& refresh_token,
    bool is_service_account)
    : login(login),
      refresh_token(refresh_token),
      is_service_account(is_service_account) {}

OAuthTokenGetter::OAuthAuthorizationCredentials::
    ~OAuthAuthorizationCredentials() = default;

// OAuthIntermediateCredentials implementation.

OAuthTokenGetter::OAuthIntermediateCredentials::OAuthIntermediateCredentials(
    const std::string& authorization_code,
    bool is_service_account)
    : authorization_code(authorization_code),
      is_service_account(is_service_account) {}

OAuthTokenGetter::OAuthIntermediateCredentials::
    ~OAuthIntermediateCredentials() = default;

}  // namespace remoting
