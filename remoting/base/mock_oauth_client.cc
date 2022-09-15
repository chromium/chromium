// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/mock_oauth_client.h"

namespace remoting {

MockOAuthClient::MockOAuthClient(const std::string& user_email,
                                 const std::string& refresh_token)
    : user_email_(user_email), refresh_token_(refresh_token) {}

MockOAuthClient::~MockOAuthClient() = default;

void MockOAuthClient::GetCredentialsFromAuthCode(
    const gaia::OAuthClientInfo& oauth_client_info,
    const std::string& auth_code,
    bool need_user_email,
    CompletionCallback on_done) {
  std::move(on_done).Run(need_user_email ? user_email_ : "", refresh_token_);
}

}  // namespace remoting
