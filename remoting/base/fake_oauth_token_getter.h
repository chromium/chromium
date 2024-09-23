// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_FAKE_OAUTH_TOKEN_GETTER_H_
#define REMOTING_BASE_FAKE_OAUTH_TOKEN_GETTER_H_

#include <string>

#include "base/functional/callback.h"
#include "remoting/base/oauth_token_getter.h"

namespace remoting {

class FakeOAuthTokenGetter : public OAuthTokenGetter {
 public:
  FakeOAuthTokenGetter(Status status,
                       const std::string& user_email,
                       const std::string& access_token,
                       const std::string& scopes);
  ~FakeOAuthTokenGetter() override;

  // OAuthTokenGetter interface.
  void CallWithToken(TokenCallback on_access_token) override;
  void InvalidateCache() override;

 private:
  Status status_;
  std::string user_email_;
  std::string access_token_;
  std::string scopes_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_FAKE_OAUTH_TOKEN_GETTER_H_
