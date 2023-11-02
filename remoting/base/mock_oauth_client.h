// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_MOCK_OAUTH_CLIENT_H_
#define REMOTING_BASE_MOCK_OAUTH_CLIENT_H_

#include "remoting/base/oauth_client.h"

namespace remoting {

class MockOAuthClient : public OAuthClient {
 public:
  MockOAuthClient(const std::string& user_email,
                  const std::string& refresh_token);

  ~MockOAuthClient() override;

  void GetCredentialsFromAuthCode(
      const gaia::OAuthClientInfo& oauth_client_info,
      const std::string& auth_code,
      bool need_user_email,
      CompletionCallback on_done) override;

 private:
  std::string user_email_;
  std::string refresh_token_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_MOCK_OAUTH_CLIENT_H_
