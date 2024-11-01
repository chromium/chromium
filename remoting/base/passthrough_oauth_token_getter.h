// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_PASSTHROUGH_OAUTH_TOKEN_GETTER_H_
#define REMOTING_BASE_PASSTHROUGH_OAUTH_TOKEN_GETTER_H_

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "remoting/base/oauth_token_getter.h"

namespace remoting {

// An OAuthTokenGetter implementation that simply passes |username| and
// |access_token| when CallWithToken() is called.
class PassthroughOAuthTokenGetter : public OAuthTokenGetter {
 public:
  // Creates a PassthroughOAuthTokenGetter with empty username and access token.
  // Caller needs to set them with set_username() and set_access_token().
  PassthroughOAuthTokenGetter();
  explicit PassthroughOAuthTokenGetter(const OAuthTokenInfo& token_info);
  ~PassthroughOAuthTokenGetter() override;

  // OAuthTokenGetter overrides.
  void CallWithToken(OAuthTokenGetter::TokenCallback on_access_token) override;
  void InvalidateCache() override;

  void set_username(const std::string& username) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    token_info_.set_user_email(username);
  }

  void set_access_token(const std::string& access_token) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    token_info_.set_access_token(access_token);
  }

  base::WeakPtr<PassthroughOAuthTokenGetter> GetWeakPtr();

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  OAuthTokenInfo token_info_;
  base::WeakPtrFactory<PassthroughOAuthTokenGetter> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_BASE_PASSTHROUGH_OAUTH_TOKEN_GETTER_H_
