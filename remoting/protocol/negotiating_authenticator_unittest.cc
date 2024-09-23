// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "net/base/net_errors.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/protocol/auth_util.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/authenticator_test_base.h"
#include "remoting/protocol/channel_authenticator.h"
#include "remoting/protocol/connection_tester.h"
#include "remoting/protocol/credentials_type.h"
#include "remoting/protocol/host_authentication_config.h"
#include "remoting/protocol/negotiating_authenticator_base.h"
#include "remoting/protocol/negotiating_client_authenticator.h"
#include "remoting/protocol/negotiating_host_authenticator.h"
#include "remoting/protocol/pairing_registry.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

using testing::_;
using testing::DeleteArg;
using testing::Return;
using testing::SaveArg;

namespace remoting::protocol {

namespace {

const int kMessageSize = 100;
const int kMessages = 1;

const char kNoClientId[] = "";
const char kNoPairedSecret[] = "";
const char kTestClientName[] = "client-name";
const char kTestClientId[] = "client-id";
const char kTestHostId[] = "12345678910123456";

const char kClientJid[] = "alice@gmail.com/abc";
const char kHostJid[] = "alice@gmail.com/123";

const char kTestPairedSecret[] = "1111-2222-3333";
const char kTestPairedSecretBad[] = "4444-5555-6666";
const char kTestPin[] = "123456";
const char kTestPinBad[] = "654321";

}  // namespace

class NegotiatingAuthenticatorTest : public AuthenticatorTestBase {
 public:
  NegotiatingAuthenticatorTest() = default;

  NegotiatingAuthenticatorTest(const NegotiatingAuthenticatorTest&) = delete;
  NegotiatingAuthenticatorTest& operator=(const NegotiatingAuthenticatorTest&) =
      delete;

  ~NegotiatingAuthenticatorTest() override = default;

 protected:
  virtual void InitAuthenticators(const std::string& client_id,
                                  const std::string& client_paired_secret,
                                  const std::string& client_interactive_pin,
                                  const std::string& host_secret) {
    std::string host_secret_hash =
        GetSharedSecretHash(kTestHostId, host_secret);
    auto auth_config =
        std::make_unique<HostAuthenticationConfig>(host_cert_, key_pair_);
    auth_config->AddPairingAuth(pairing_registry_);
    auth_config->AddSharedSecretAuth(host_secret_hash);
    auto host = std::make_unique<NegotiatingHostAuthenticator>(
        kHostJid, kClientJid, std::move(auth_config));
    host_as_negotiating_authenticator_ = host.get();
    host_ = std::move(host);

    protocol::ClientAuthenticationConfig client_auth_config;
    client_auth_config.host_id = kTestHostId;
    client_auth_config.pairing_client_id = client_id;
    client_auth_config.pairing_secret = client_paired_secret;
    bool pairing_expected = pairing_registry_.get() != nullptr;
    client_auth_config.fetch_secret_callback =
        base::BindRepeating(&NegotiatingAuthenticatorTest::FetchSecret,
                            client_interactive_pin, pairing_expected);
    client_as_negotiating_authenticator_ = new NegotiatingClientAuthenticator(
        kClientJid, kHostJid, client_auth_config);
    client_.reset(client_as_negotiating_authenticator_);
  }

  void DisableMethodOnClient(AuthenticationMethod method) {
    auto* methods = &(client_as_negotiating_authenticator_->methods_);
    auto iter = base::ranges::find(*methods, method);
    ASSERT_TRUE(iter != methods->end());
    methods->erase(iter);
  }

  void DisableMethodOnHost(AuthenticationMethod method) {
    auto* methods = &(host_as_negotiating_authenticator_->methods_);
    auto iter = base::ranges::find(*methods, method);
    ASSERT_TRUE(iter != methods->end());
    methods->erase(iter);
  }

  void CreatePairingRegistry(bool with_paired_client) {
    pairing_registry_ = new SynchronousPairingRegistry(
        std::make_unique<MockPairingRegistryDelegate>());
    if (with_paired_client) {
      PairingRegistry::Pairing pairing(base::Time(), kTestClientName,
                                       kTestClientId, kTestPairedSecret);
      pairing_registry_->AddPairing(pairing);
    }
  }

  void SwapCurrentAuthenticator(
      NegotiatingAuthenticatorBase* negotiating_authenticator,
      std::unique_ptr<Authenticator> current_authenticator) {
    negotiating_authenticator->current_authenticator_ =
        std::move(current_authenticator);
    negotiating_authenticator->ChainStateChangeAfterAcceptedWithUnderlying(
        *negotiating_authenticator->current_authenticator_);
  }

  static void FetchSecret(
      const std::string& client_secret,
      bool pairing_supported,
      bool pairing_expected,
      const protocol::SecretFetchedCallback& secret_fetched_callback) {
    secret_fetched_callback.Run(client_secret);
    ASSERT_EQ(pairing_supported, pairing_expected);
  }

  void VerifyRejected(Authenticator::RejectionReason reason) {
    ASSERT_TRUE(client_->state() == Authenticator::REJECTED ||
                host_->state() == Authenticator::REJECTED);
    if (client_->state() == Authenticator::REJECTED) {
      ASSERT_EQ(client_->rejection_reason(), reason);
    }
    if (host_->state() == Authenticator::REJECTED) {
      ASSERT_EQ(host_->rejection_reason(), reason);
    }
  }

  virtual void VerifyAccepted() {
    ASSERT_NO_FATAL_FAILURE(RunAuthExchange());

    ASSERT_EQ(Authenticator::ACCEPTED, host_->state());
    ASSERT_EQ(Authenticator::ACCEPTED, client_->state());

    client_auth_ = client_->CreateChannelAuthenticator();
    host_auth_ = host_->CreateChannelAuthenticator();
    RunChannelAuth(false);

    EXPECT_TRUE(client_socket_.get() != nullptr);
    EXPECT_TRUE(host_socket_.get() != nullptr);

    StreamConnectionTester tester(host_socket_.get(), client_socket_.get(),
                                  kMessageSize, kMessages);

    base::RunLoop run_loop;
    tester.Start(run_loop.QuitClosure());
    run_loop.Run();
    tester.CheckResults();
  }

  AuthenticationMethod current_method() {
    return client_as_negotiating_authenticator_->current_method_;
  }

  // Use a bare pointer because the storage is managed by the base class.
  raw_ptr<NegotiatingHostAuthenticator> host_as_negotiating_authenticator_;
  raw_ptr<NegotiatingClientAuthenticator> client_as_negotiating_authenticator_;

 private:
  scoped_refptr<PairingRegistry> pairing_registry_;
};

class NegotiatingPairingAuthenticatorTest
    : public NegotiatingAuthenticatorTest {
 public:
  void VerifyAccepted() override {
    NegotiatingAuthenticatorTest::VerifyAccepted();
    EXPECT_EQ(current_method(), AuthenticationMethod::PAIRED_SPAKE2_CURVE25519);
  }
};

TEST_F(NegotiatingAuthenticatorTest, SuccessfulAuthSharedSecret) {
  ASSERT_NO_FATAL_FAILURE(
      InitAuthenticators(kNoClientId, kNoPairedSecret, kTestPin, kTestPin));
  VerifyAccepted();
  EXPECT_EQ(AuthenticationMethod::SHARED_SECRET_SPAKE2_CURVE25519,
            current_method());
}

TEST_F(NegotiatingAuthenticatorTest, InvalidSharedSecret) {
  ASSERT_NO_FATAL_FAILURE(
      InitAuthenticators(kNoClientId, kNoPairedSecret, kTestPinBad, kTestPin));
  ASSERT_NO_FATAL_FAILURE(RunAuthExchange());

  VerifyRejected(Authenticator::RejectionReason::INVALID_CREDENTIALS);
}

TEST_F(NegotiatingAuthenticatorTest, NoCommonAuthMethod) {
  ASSERT_NO_FATAL_FAILURE(
      InitAuthenticators(kNoClientId, kNoPairedSecret, kTestPin, kTestPinBad));
  DisableMethodOnClient(AuthenticationMethod::SHARED_SECRET_SPAKE2_CURVE25519);
  DisableMethodOnHost(AuthenticationMethod::SHARED_SECRET_SPAKE2_CURVE25519);

  ASSERT_NO_FATAL_FAILURE(RunAuthExchange());

  VerifyRejected(Authenticator::RejectionReason::NO_COMMON_AUTH_METHOD);
}

TEST_F(NegotiatingAuthenticatorTest, PairingNotSupported) {
  ASSERT_NO_FATAL_FAILURE(
      InitAuthenticators(kTestClientId, kTestPairedSecret, kTestPin, kTestPin));
  ASSERT_NO_FATAL_FAILURE(RunAuthExchange());
  VerifyAccepted();
  EXPECT_EQ(AuthenticationMethod::SHARED_SECRET_SPAKE2_CURVE25519,
            current_method());
}

TEST_F(NegotiatingPairingAuthenticatorTest, PairingSupportedButNotPaired) {
  CreatePairingRegistry(false);
  ASSERT_NO_FATAL_FAILURE(
      InitAuthenticators(kNoClientId, kNoPairedSecret, kTestPin, kTestPin));
  ASSERT_NO_FATAL_FAILURE(RunAuthExchange());
  VerifyAccepted();
}

TEST_F(NegotiatingPairingAuthenticatorTest, PairingRevokedPinOkay) {
  CreatePairingRegistry(false);
  ASSERT_NO_FATAL_FAILURE(
      InitAuthenticators(kTestClientId, kTestPairedSecret, kTestPin, kTestPin));
  ASSERT_NO_FATAL_FAILURE(RunAuthExchange());
  VerifyAccepted();
}

TEST_F(NegotiatingPairingAuthenticatorTest, PairingRevokedPinBad) {
  CreatePairingRegistry(false);
  ASSERT_NO_FATAL_FAILURE(InitAuthenticators(kTestClientId, kTestPairedSecret,
                                             kTestPinBad, kTestPin));
  ASSERT_NO_FATAL_FAILURE(RunAuthExchange());
  VerifyRejected(Authenticator::RejectionReason::INVALID_CREDENTIALS);
}

TEST_F(NegotiatingPairingAuthenticatorTest, PairingSucceeded) {
  CreatePairingRegistry(true);
  ASSERT_NO_FATAL_FAILURE(InitAuthenticators(kTestClientId, kTestPairedSecret,
                                             kTestPinBad, kTestPin));
  ASSERT_NO_FATAL_FAILURE(RunAuthExchange());
  VerifyAccepted();
}

TEST_F(NegotiatingPairingAuthenticatorTest,
       PairingSucceededInvalidSecretButPinOkay) {
  CreatePairingRegistry(true);
  ASSERT_NO_FATAL_FAILURE(InitAuthenticators(
      kTestClientId, kTestPairedSecretBad, kTestPin, kTestPin));
  ASSERT_NO_FATAL_FAILURE(RunAuthExchange());
  VerifyAccepted();
}

TEST_F(NegotiatingPairingAuthenticatorTest, PairingFailedInvalidSecretAndPin) {
  CreatePairingRegistry(true);
  ASSERT_NO_FATAL_FAILURE(InitAuthenticators(
      kTestClientId, kTestPairedSecretBad, kTestPinBad, kTestPin));
  ASSERT_NO_FATAL_FAILURE(RunAuthExchange());
  VerifyRejected(Authenticator::RejectionReason::INVALID_CREDENTIALS);
}

TEST_F(NegotiatingAuthenticatorTest, NotifyStateChangeAfterAccepted) {
  base::MockRepeatingClosure host_state_change_after_accepted;
  base::MockRepeatingClosure client_state_change_after_accepted;
  ASSERT_NO_FATAL_FAILURE(
      InitAuthenticators(kNoClientId, kNoPairedSecret, kTestPin, kTestPin));
  host_->set_state_change_after_accepted_callback(
      host_state_change_after_accepted.Get());
  client_->set_state_change_after_accepted_callback(
      client_state_change_after_accepted.Get());
  VerifyAccepted();

  // There is no client authenticator for SessionAuthz (which has
  // state-change-after-accepted behavior), so we have to swap the current
  // authenticators with mock ones.
  auto mock_host_authenticator_owned = std::make_unique<MockAuthenticator>();
  auto mock_client_authenticator_owned = std::make_unique<MockAuthenticator>();
  MockAuthenticator* mock_host_authenticator =
      mock_host_authenticator_owned.get();
  MockAuthenticator* mock_client_authenticator =
      mock_client_authenticator_owned.get();
  SwapCurrentAuthenticator(host_as_negotiating_authenticator_,
                           std::move(mock_host_authenticator_owned));
  SwapCurrentAuthenticator(client_as_negotiating_authenticator_,
                           std::move(mock_client_authenticator_owned));
  EXPECT_CALL(*mock_host_authenticator, state())
      .WillOnce(Return(Authenticator::REJECTED));
  EXPECT_CALL(*mock_client_authenticator, state())
      .WillOnce(Return(Authenticator::REJECTED));
  EXPECT_CALL(*mock_host_authenticator, rejection_reason())
      .WillOnce(
          Return(Authenticator::RejectionReason::REAUTHZ_POLICY_CHECK_FAILED));
  EXPECT_CALL(*mock_client_authenticator, rejection_reason())
      .WillOnce(
          Return(Authenticator::RejectionReason::REAUTHZ_POLICY_CHECK_FAILED));
  EXPECT_CALL(host_state_change_after_accepted, Run());
  EXPECT_CALL(client_state_change_after_accepted, Run());

  mock_host_authenticator->NotifyStateChangeAfterAccepted();
  mock_client_authenticator->NotifyStateChangeAfterAccepted();

  EXPECT_EQ(host_->state(), Authenticator::REJECTED);
  EXPECT_EQ(client_->state(), Authenticator::REJECTED);
  EXPECT_EQ(host_->rejection_reason(),
            Authenticator::RejectionReason::REAUTHZ_POLICY_CHECK_FAILED);
  EXPECT_EQ(client_->rejection_reason(),
            Authenticator::RejectionReason::REAUTHZ_POLICY_CHECK_FAILED);
}

TEST_F(NegotiatingAuthenticatorTest,
       ReturnCorrectCredentialsTypeAndImplementingAuthenticator) {
  InitAuthenticators(kNoClientId, kNoPairedSecret, kTestPin, kTestPin);

  ASSERT_EQ(host_->credentials_type(), CredentialsType::UNKNOWN);
  ASSERT_EQ(&host_->implementing_authenticator(), host_.get());
  VerifyAccepted();
  ASSERT_EQ(host_->credentials_type(), CredentialsType::SHARED_SECRET);
  ASSERT_NE(&host_->implementing_authenticator(), host_.get());
}

}  // namespace remoting::protocol
