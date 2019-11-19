// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_TEST_OAUTH_TOKEN_GETTER_H_
#define REMOTING_TEST_TEST_OAUTH_TOKEN_GETTER_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "remoting/base/oauth_token_getter.h"

namespace network {
class TransitionalURLLoaderFactoryOwner;
}  // namespace network

namespace remoting {
namespace test {
class TestTokenStorage;

// An OAuthTokenGetter implementation for testing that runs the authentication
// flow on the console.
// If the account is whitelisted to use 1P scope with consent page then it will
// store the refresh token, otherwise it will just cache the access token, which
// will expire in ~1h.
class TestOAuthTokenGetter final : public OAuthTokenGetter {
 public:
  static constexpr char kSwitchNameAuthCode[] = "auth-code";

  static bool IsServiceAccount(const std::string& email);

  // |token_storage| must outlive |this|.
  explicit TestOAuthTokenGetter(TestTokenStorage* token_storage);
  ~TestOAuthTokenGetter() override;

  // Initializes the token getter and runs the authentication flow on the
  // console if necessary.
  void Initialize(base::OnceClosure on_done);

  // Ignores auth token cache and runs the authentication flow on the console.
  // Similar to InvalidateCache() but takes a callback.
  void ResetWithAuthenticationFlow(base::OnceClosure on_done);

  // OAuthTokenGetter implementations
  void CallWithToken(TokenCallback on_access_token) override;
  void InvalidateCache() override;

  base::WeakPtr<TestOAuthTokenGetter> GetWeakPtr();

 private:
  std::unique_ptr<OAuthTokenGetter> CreateFromIntermediateCredentials(
      const std::string& auth_code,
      const OAuthTokenGetter::CredentialsUpdatedCallback&
          on_credentials_update);
  std::unique_ptr<OAuthTokenGetter> CreateWithRefreshToken(
      const std::string& refresh_token,
      const std::string& email);

  void OnCredentialsUpdate(const std::string& user_email,
                           const std::string& refresh_token);

  void OnAccessToken(OAuthTokenGetter::Status status,
                     const std::string& user_email,
                     const std::string& access_token);

  void RunAuthenticationDoneCallbacks();

  std::unique_ptr<network::TransitionalURLLoaderFactoryOwner>
      url_loader_factory_owner_;
  TestTokenStorage* token_storage_ = nullptr;
  std::unique_ptr<OAuthTokenGetter> token_getter_;
  bool is_authenticating_ = false;
  base::queue<base::OnceClosure> on_authentication_done_;

  base::WeakPtrFactory<TestOAuthTokenGetter> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(TestOAuthTokenGetter);
};

}  // namespace test
}  // namespace remoting

#endif  // REMOTING_TEST_TEST_OAUTH_TOKEN_GETTER_H_
