// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_OAUTH_TOKEN_INFO_H_
#define REMOTING_BASE_OAUTH_TOKEN_INFO_H_

#include <string>

namespace remoting {

// OAuthTokenInfo contains relevant info for a given OAuth token.
struct OAuthTokenInfo {
  OAuthTokenInfo() = default;
  explicit OAuthTokenInfo(const std::string& token);
  OAuthTokenInfo(const std::string& token, const std::string& email);

  std::string access_token() const { return access_token_; }
  std::string user_email() const { return user_email_; }

  void set_access_token(const std::string& access_token) {
    access_token_ = access_token;
  }
  void set_user_email(const std::string& user_email) {
    user_email_ = user_email;
  }

 private:
  std::string access_token_;
  std::string user_email_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_OAUTH_TOKEN_INFO_H_
