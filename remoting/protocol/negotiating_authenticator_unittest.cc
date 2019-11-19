// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "net/base/net_errors.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/protocol/auth_util.h"
#include "remoting/protocol/authenticator_test_base.h"
#include "remoting/protocol/channel_authenticator.h"
#include "remoting/protocol/connection_tester.h"
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
using testing::SaveArg;

namespace remoting {
namespace protocol {

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
  ~NegotiatingAuthenticatorTest() override = default;

 protected:
  virtual void InitAuthenticators(const std::string& client_id,
                                  const std::string& client_paired_secret,
                                  const std::string& client_interactive_pin,
                                  const std::string& host_secret) {
    std::string host_secret_hash =
        GetSharedSecretHash(kTestHostId, host_secret);
    std::unique_ptr<NegotiatingHostAuthenticator> host =
        NegotiatingHostAuthenticator::CreateWithSharedSecret(
            kHostJid, kClientJid, host_cert_, key_pair_, host_secret_hash,
            pairing_registry_);
    host_as_negotiating_authenticator_ = host.get();
    host_ = std::move(host);

    protocol::ClientAuthenticationConfig client_auth_config;
    client_auth_config.host_id = kTestHostId;
    client_auth_config.pairing_client_id = client_id;
    client_auth_config.pairing_secret = client_paired_secret;
    bool pairing_expected = pairing_registry_.get() != nullptr;
    client_auth_config.fetch_secret_callback =
        base::Bind(&NegotiatingAuthenticatorTest::FetchSecret,
                   client_interactive_pin, pairing_expected);
    client_as_negotiating_authenticator_ = new NegotiatingClientAuthenticator(
        kClientJid, kHostJid, client_auth_config);
    client_.reset(client_as_negotiating_authenticator_);
  }

  void DisableMethodOnClient(NegotiatingAuthenticatorBase::Method method) {
    auto* methods = &(client_as_negotiating_authenticator_->methods_);
    auto iter = std::find(methods->begin(), methods->end(), method);
    ASSERT_TRUE(iter != methods->end());
    methods->erase(iter);
  }

  void DisableMethodOnHost(NegotiatingAuthenticatorBase::Method method) {
    auto* methods = &(host_as_negotiating_authenticator_->methods_);
    auto iter = std::find(methods->begin(), methods->end(), method);
    ASSERT_TRUE(iter != methods->end());
    methods->erase(iter);
  }

  void CreatePairingRegistry(bool with_paired_client) {
    pairing_registry_ = new SynchronousPairingRegistry(
        std::make_unique<MockPairingRegistryDelegate>());
    if (with_paired_client) {
      PairingRegistry::Pairing pairing(
          base::Time(), kTestClientName, kTestClientId, kTestPairedSecret);
      pairing_registry_->AddPairing(pairing);
    }
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

  NegotiatingAuthenticatorBase::Method current_method() {
    return client_as_negotiating_authenticator_->current_method_;
  }

  // Use a bare pointer because the storage is managed by the base class.
  NegotiatingHostAuthenticator* host_as_negotiating_authenticator_;
  NegotiatingClientAuthenticator* client_as_negotiating_authenticator_;

 private:
  scoped_refptr<PairingRegistry> pairing_registry_;

  DISALLOW_COPY_AND_ASSIGN(NegotiatingAuthenticatorTest);
};

struct PairingTestParameters {
  bool p224_on_client;
  bool curve25519_on_client;
  bool p224_on_host;
  bool curve25519_on_host;

  bool expect_curve25519_used;
};

class NegotiatingPairingAuthenticatorTest
    : public NegotiatingAuthenticatorTest,
      public testing::WithParamInterface<PairingTestParameters> {
public:
 void InitAuthenticators(const std::string& client_id,
                         const std::string& client_paired_secret,
                         const std::string& client_interactive_pin,
                         const std::string& host_secret) override {
   NegotiatingAuthenticatorTest::InitAuthenticators(
       client_id, client_paired_secret, client_interactive_pin, host_secret);
   if (!GetParam().p224_on_client) {
     DisableMethodOnClient(
         NegotiatingAuthenticatorBase::Method::PAIRED_SPAKE2_P224);
   }
   if (!GetParam().curve25519_on_client) {
     DisableMethodOnClient(
         NegotiatingAuthenticatorBase::Method::PAIRED_SPAKE2_CURVE25519);
   }
   if (!GetParam().p224_on_host) {
     DisableMethodOnHost(
         NegotiatingAuthenticatorBase::Method::PAIRED_SPAKE2_P224);
   }
   if (!GetParam().curve25519_on_host) {
     DisableMethodOnHost(
         NegotiatingAuthenticatorBase::Method::PAIRED_SPAKE2_CURVE25519);
   }
 }

 void VerifyAccepted() override {
   NegotiatingAuthenticatorTest::VerifyAccepted();
   EXPECT_TRUE(
       current_method() ==
           NegotiatingAuthenticatorBase::Method::PAIRED_SPAKE2_P224 ||
       current_method() ==
           NegotiatingAuthenticatorBase::Method::PAIRED_SPAKE2_CURVE25519);
 }
};

INSTANTIATE_TEST_SUITE_P(
    PairingParams,
    NegotiatingPairingAuthenticatorTest,
    testing::Values(
        // Only P224.
        PairingTestParameters{true, false, true, false},

        // Only curve25519.
        PairingTestParameters{false, true, false, true},

        // Both P224 and curve25519.
        PairingTestParameters{true, true, true, true},

        // One end supports both, the other supports only P224 or curve25519.
        PairingTestParameters{false, true, true, true},
        PairingTestParameters{true, false, true, true},
        PairingTestParameters{true, true, false, true},
        PairingTestParameters{true, true, true, false}));

TEST_F(NegotiatingAuthenticatorTest, SuccessfulAuthSharedSecret) {
  ASSERT_NO_FATAL_FAILURE(InitAuthenticators(kNoClientId, kNoPairedSecret,
                                             kTestPin, kTestPin));
  VerifyAccepted();
  EXPECT_EQ(
      NegotiatingAuthenticatorBase::Method::SHARED_SECRET_SPAKE2_CURVE25519,
      current_method());
}

TEST_F(NegotiatingAuthenticatorTest, InvalidSharedSecret) {
  ASSERT_NO_FATAL_FAILURE(InitAuthenticators(kNoClientId, kNoPairedSecret,
                                             kTestPinBad, kTestPin));
  ASSERT_NO_FATAL_FAILURE(RunAuthExchange());

  VerifyRejected(Authenticator::INVALID_CREDENTIALS);
}

TEST_F(NegotiatingAuthenticatorTest, IncompatibleMethods) {
  ASSERT_NO_FATAL_FAILURE(
      InitAuthenticators(kNoClientId, kNoPairedSecret, kTestPin, kTestPinBad));
  DisableMethodOnClient(
      NegotiatingAuthenticatorBase::Method::SHARED_SECRET_SPAKE2_P224);
  DisableMethodOnHost(
      NegotiatingAuthenticatorBase::Method::SHARED_SECRET_SPAKE2_CURVE25519);

  ASSERT_NO_FATAL_FAILURE(RunAuthExchange());

  VerifyRejected(Authenticator::PROTOCOL_ERROR);
}

TEST_F(NegotiatingAuthenticatorTest, PairingNotSupported) {
  ASSERT_NO_FATAL_FAILURE(
      InitAuthenticators(kTestClientId, kTestPairedSecret, kTestPin, kTestPin));
  ASSERT_NO_FATAL_FAILURE(RunAuthExchange());
  VerifyAccepted();
  EXPECT_EQ(
      NegotiatingAuthenticatorBase::Method::SHARED_SECRET_SPAKE2_CURVE25519,
      current_method());
}

TEST_P(NegotiatingPairingAuthenticatorTest, PairingSupportedButNotPaired) {
  CreatePairingRegistry(false);
  ASSERT_NO_FATAL_FAILURE(
      InitAuthenticators(kNoClientId, kNoPairedSecret, kTestPin, kTestPin));
  ASSERT_NO_FATAL_FAILURE(RunAuthExchange());
  VerifyAccepted();
}

TEST_P(NegotiatingPairingAuthenticatorTest, PairingRevokedPinOkay) {
  CreatePairingRegistry(false);
  ASSERT_NO_FATAL_FAILURE(
      InitAuthenticators(kTestClientId, kTestPairedSecret, kTestPin, kTestPin));
  ASSERT_NO_FATAL_FAILURE(RunAuthExchange());
  VerifyAccepted();
}

TEST_P(NegotiatingPairingAuthenticatorTest, PairingRevokedPinBad) {
  CreatePairingRegistry(false);
  ASSERT_NO_FATAL_FAILURE(InitAuthenticators(kTestClientId, kTestPairedSecret,
                                             kTestPinBad, kTestPin));
  ASSERT_NO_FATAL_FAILURE(RunAuthExchange());
  VerifyRejected(Authenticator::INVALID_CREDENTIALS);
}

TEST_P(NegotiatingPairingAuthenticatorTest, PairingSucceeded) {
  CreatePairingRegistry(true);
  ASSERT_NO_FATAL_FAILURE(InitAuthenticators(kTestClientId, kTestPairedSecret,
                                             kTestPinBad, kTestPin));
  ASSERT_NO_FATAL_FAILURE(RunAuthExchange());
  VerifyAccepted();
}

TEST_P(NegotiatingPairingAuthenticatorTest,
       PairingSucceededInvalidSecretButPinOkay) {
  CreatePairingRegistry(true);
  ASSERT_NO_FATAL_FAILURE(InitAuthenticators(
      kTestClientId, kTestPairedSecretBad, kTestPin, kTestPin));
  ASSERT_NO_FATAL_FAILURE(RunAuthExchange());
  VerifyAccepted();
}

TEST_P(NegotiatingPairingAuthenticatorTest, PairingFailedInvalidSecretAndPin) {
  CreatePairingRegistry(true);
  ASSERT_NO_FATAL_FAILURE(InitAuthenticators(
      kTestClientId, kTestPairedSecretBad, kTestPinBad, kTestPin));
  ASSERT_NO_FATAL_FAILURE(RunAuthExchange());
  VerifyRejected(Authenticator::INVALID_CREDENTIALS);
}

}  // namespace protocol
}  // namespace remoting
