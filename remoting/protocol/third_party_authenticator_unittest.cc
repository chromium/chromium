// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "net/base/net_errors.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/protocol/authenticator_test_base.h"
#include "remoting/protocol/channel_authenticator.h"
#include "remoting/protocol/connection_tester.h"
#include "remoting/protocol/fake_authenticator.h"
#include "remoting/protocol/third_party_authenticator_base.h"
#include "remoting/protocol/third_party_client_authenticator.h"
#include "remoting/protocol/third_party_host_authenticator.h"
#include "remoting/protocol/token_validator.h"
#include "remoting/protocol/v2_authenticator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

using testing::_;
using testing::DeleteArg;
using testing::SaveArg;

namespace {

const int kMessageSize = 100;
const int kMessages = 1;

const char kTokenUrl[] = "https://example.com/Issue";
const char kTokenScope[] = "host:a@b.com/1 client:a@b.com/2";
const char kToken[] = "abc123456xyz789";
const char kSharedSecret[] = "1234-1234-5678";
const char kSharedSecretBad[] = "0000-0000-0001";

}  // namespace

namespace remoting {
namespace protocol {

class ThirdPartyAuthenticatorTest : public AuthenticatorTestBase {
  class FakeTokenFetcher {
   public:
    void FetchThirdPartyToken(
        const std::string& token_url,
        const std::string& scope,
        const ThirdPartyTokenFetchedCallback& token_fetched_callback) {
      ASSERT_EQ(token_url, kTokenUrl);
      ASSERT_EQ(scope, kTokenScope);
      ASSERT_FALSE(token_fetched_callback.is_null());
      on_token_fetched_ = token_fetched_callback;
    }

    void OnTokenFetched(const std::string& token,
                        const std::string& shared_secret) {
      ASSERT_FALSE(on_token_fetched_.is_null());
      std::move(on_token_fetched_).Run(token, shared_secret);
    }

   private:
    ThirdPartyTokenFetchedCallback on_token_fetched_;
  };

  class FakeTokenValidator : public TokenValidator {
   public:
    FakeTokenValidator()
     : token_url_(kTokenUrl),
       token_scope_(kTokenScope) {}

    ~FakeTokenValidator() override = default;

    void ValidateThirdPartyToken(
        const std::string& token,
        const TokenValidatedCallback& token_validated_callback) override {
      ASSERT_FALSE(token_validated_callback.is_null());
      on_token_validated_ = token_validated_callback;
    }

    void OnTokenValidated(const std::string& shared_secret) {
      ASSERT_FALSE(on_token_validated_.is_null());
      std::move(on_token_validated_).Run(shared_secret);
    }

    const GURL& token_url() const override { return token_url_; }

    const std::string& token_scope() const override { return token_scope_; }

   private:
    GURL token_url_;
    std::string token_scope_;
    base::Callback<void(const std::string& shared_secret)> on_token_validated_;
  };

 public:
  ThirdPartyAuthenticatorTest() = default;
  ~ThirdPartyAuthenticatorTest() override = default;

 protected:
  void InitAuthenticators() {
    token_validator_ = new FakeTokenValidator();
    host_.reset(new ThirdPartyHostAuthenticator(
        base::Bind(&V2Authenticator::CreateForHost, host_cert_, key_pair_),
        base::WrapUnique(token_validator_)));
    client_.reset(new ThirdPartyClientAuthenticator(
        base::Bind(&V2Authenticator::CreateForClient),
        base::Bind(&FakeTokenFetcher::FetchThirdPartyToken,
                   base::Unretained(&token_fetcher_))));
  }

  FakeTokenFetcher token_fetcher_;
  FakeTokenValidator* token_validator_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ThirdPartyAuthenticatorTest);
};

TEST_F(ThirdPartyAuthenticatorTest, SuccessfulAuth) {
  ASSERT_NO_FATAL_FAILURE(InitAuthenticators());
  ASSERT_NO_FATAL_FAILURE(RunHostInitiatedAuthExchange());
  ASSERT_EQ(Authenticator::PROCESSING_MESSAGE, client_->state());
  ASSERT_NO_FATAL_FAILURE(token_fetcher_.OnTokenFetched(kToken, kSharedSecret));
  ASSERT_EQ(Authenticator::PROCESSING_MESSAGE, host_->state());
  ASSERT_NO_FATAL_FAILURE(token_validator_->OnTokenValidated(kSharedSecret));

  // Both sides have finished.
  ASSERT_EQ(Authenticator::ACCEPTED, host_->state());
  ASSERT_EQ(Authenticator::ACCEPTED, client_->state());

  // An authenticated channel can be created after the authentication.
  client_auth_ = client_->CreateChannelAuthenticator();
  host_auth_ = host_->CreateChannelAuthenticator();
  RunChannelAuth(false);

  StreamConnectionTester tester(host_socket_.get(), client_socket_.get(),
                                kMessageSize, kMessages);

  base::RunLoop run_loop;
  tester.Start(run_loop.QuitClosure());
  run_loop.Run();
  tester.CheckResults();
}

TEST_F(ThirdPartyAuthenticatorTest, ClientNoSecret) {
  ASSERT_NO_FATAL_FAILURE(InitAuthenticators());
  ASSERT_NO_FATAL_FAILURE(RunHostInitiatedAuthExchange());
  ASSERT_EQ(Authenticator::PROCESSING_MESSAGE, client_->state());
  ASSERT_NO_FATAL_FAILURE(token_fetcher_.OnTokenFetched(kToken, std::string()));

  // The end result is that the client rejected the connection, since it
  // couldn't fetch the secret.
  ASSERT_EQ(Authenticator::REJECTED, client_->state());
}

TEST_F(ThirdPartyAuthenticatorTest, InvalidToken) {
  ASSERT_NO_FATAL_FAILURE(InitAuthenticators());
  ASSERT_NO_FATAL_FAILURE(RunHostInitiatedAuthExchange());
  ASSERT_EQ(Authenticator::PROCESSING_MESSAGE, client_->state());
  ASSERT_NO_FATAL_FAILURE(token_fetcher_.OnTokenFetched(
      kToken, kSharedSecret));
  ASSERT_EQ(Authenticator::PROCESSING_MESSAGE, host_->state());
  ASSERT_NO_FATAL_FAILURE(token_validator_->OnTokenValidated(std::string()));

  // The end result is that the host rejected the token.
  ASSERT_EQ(Authenticator::REJECTED, host_->state());
}

TEST_F(ThirdPartyAuthenticatorTest, CannotFetchToken) {
  ASSERT_NO_FATAL_FAILURE(InitAuthenticators());
  ASSERT_NO_FATAL_FAILURE(RunHostInitiatedAuthExchange());
  ASSERT_EQ(Authenticator::PROCESSING_MESSAGE, client_->state());
  ASSERT_NO_FATAL_FAILURE(
      token_fetcher_.OnTokenFetched(std::string(), std::string()));

  // The end result is that the client rejected the connection, since it
  // couldn't fetch the token.
  ASSERT_EQ(Authenticator::REJECTED, client_->state());
}

// Test that negotiation stops when the fake authentication is rejected.
TEST_F(ThirdPartyAuthenticatorTest, HostBadSecret) {
  ASSERT_NO_FATAL_FAILURE(InitAuthenticators());
  ASSERT_NO_FATAL_FAILURE(RunHostInitiatedAuthExchange());
  ASSERT_EQ(Authenticator::PROCESSING_MESSAGE, client_->state());
  ASSERT_NO_FATAL_FAILURE(token_fetcher_.OnTokenFetched(kToken, kSharedSecret));
  ASSERT_EQ(Authenticator::PROCESSING_MESSAGE, host_->state());
  ASSERT_NO_FATAL_FAILURE(
      token_validator_->OnTokenValidated(kSharedSecretBad));

  // The end result is that the host rejected the fake authentication.
  ASSERT_EQ(Authenticator::REJECTED, client_->state());
}

TEST_F(ThirdPartyAuthenticatorTest, ClientBadSecret) {
  ASSERT_NO_FATAL_FAILURE(InitAuthenticators());
  ASSERT_NO_FATAL_FAILURE(RunHostInitiatedAuthExchange());
  ASSERT_EQ(Authenticator::PROCESSING_MESSAGE, client_->state());
  ASSERT_NO_FATAL_FAILURE(
      token_fetcher_.OnTokenFetched(kToken, kSharedSecretBad));
  ASSERT_EQ(Authenticator::PROCESSING_MESSAGE, host_->state());
  ASSERT_NO_FATAL_FAILURE(
      token_validator_->OnTokenValidated(kSharedSecret));

  // The end result is that the host rejected the fake authentication.
  ASSERT_EQ(Authenticator::REJECTED, client_->state());
}

}  // namespace protocol
}  // namespace remoting
