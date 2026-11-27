// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/ssl_private_key_proxy.h"

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace network {
namespace {

using ::testing::IsEmpty;

// Fake `mojom::SSLPrivateKey` implementation that captures incoming `Sign()`
// requests without replying immediately so tests can control completion or
// close the receiver pipe while a request is in flight.
class DeferredMojoSSLPrivateKey : public mojom::SSLPrivateKey {
 public:
  DeferredMojoSSLPrivateKey() = default;
  ~DeferredMojoSSLPrivateKey() override = default;

  mojo::PendingRemote<mojom::SSLPrivateKey> BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void ClosePipe() { receiver_.reset(); }

  void WaitForSignRequest() { sign_requested_future_.Get(); }

  uint16_t last_algorithm() const { return last_algorithm_; }
  const std::vector<uint8_t>& last_input() const { return last_input_; }

  void CompleteSign(int32_t net_error, const std::vector<uint8_t>& signature) {
    ASSERT_TRUE(pending_callback_);
    std::move(pending_callback_).Run(net_error, signature);
  }

  // mojom::SSLPrivateKey:
  void Sign(uint16_t algorithm,
            const std::vector<uint8_t>& input,
            SignCallback callback) override {
    last_algorithm_ = algorithm;
    last_input_ = input;
    pending_callback_ = std::move(callback);
    sign_requested_future_.SetValue();
  }

 private:
  mojo::Receiver<mojom::SSLPrivateKey> receiver_{this};
  uint16_t last_algorithm_ = 0;
  std::vector<uint8_t> last_input_;
  SignCallback pending_callback_;
  base::test::TestFuture<void> sign_requested_future_;
};

class SSLPrivateKeyProxyTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
};

TEST_F(SSLPrivateKeyProxyTest, SignSuccess) {
  DeferredMojoSSLPrivateKey impl;
  const std::vector<uint16_t> kAlgs = {SSL_SIGN_RSA_PSS_RSAE_SHA256,
                                       SSL_SIGN_RSA_PKCS1_SHA256};
  auto proxy = base::MakeRefCounted<SSLPrivateKeyProxy>(
      "TestProvider", kAlgs, impl.BindNewPipeAndPassRemote());

  EXPECT_EQ(proxy->GetProviderName(), "TestProvider");
  EXPECT_EQ(proxy->GetAlgorithmPreferences(), kAlgs);

  const std::vector<uint8_t> kInput = {1, 2, 3, 4};
  const std::vector<uint8_t> kSignature = {9, 8, 7, 6};

  base::test::TestFuture<net::Error, const std::vector<uint8_t>&> sign_future;
  proxy->Sign(SSL_SIGN_RSA_PSS_RSAE_SHA256, kInput, sign_future.GetCallback());

  impl.WaitForSignRequest();
  EXPECT_EQ(impl.last_algorithm(), SSL_SIGN_RSA_PSS_RSAE_SHA256);
  EXPECT_EQ(impl.last_input(), kInput);
  EXPECT_FALSE(sign_future.IsReady());

  impl.CompleteSign(net::OK, kSignature);
  EXPECT_EQ(sign_future.Get<0>(), net::OK);
  EXPECT_EQ(sign_future.Get<1>(), kSignature);
}

TEST_F(SSLPrivateKeyProxyTest, SignWhenPipeAlreadyDisconnected) {
  DeferredMojoSSLPrivateKey impl;
  auto proxy = base::MakeRefCounted<SSLPrivateKeyProxy>(
      "TestProvider", std::vector<uint16_t>{SSL_SIGN_RSA_PSS_RSAE_SHA256},
      impl.BindNewPipeAndPassRemote());

  // Disconnect the Mojo pipe and observe the disconnect handler running (via
  // an initial Sign() failing with ERR_SSL_CLIENT_AUTH_CERT_NO_PRIVATE_KEY)
  // before issuing the subsequent Sign() call.
  const std::vector<uint8_t> kInput = {1, 2, 3, 4};
  base::test::TestFuture<net::Error, const std::vector<uint8_t>&>
      disconnect_future;
  proxy->Sign(SSL_SIGN_RSA_PSS_RSAE_SHA256, kInput,
              disconnect_future.GetCallback());
  impl.ClosePipe();
  ASSERT_EQ(disconnect_future.Get<0>(),
            net::ERR_SSL_CLIENT_AUTH_CERT_NO_PRIVATE_KEY);

  bool callback_ran_synchronously = false;
  base::test::TestFuture<net::Error, const std::vector<uint8_t>&> sign_future;
  proxy->Sign(
      SSL_SIGN_RSA_PSS_RSAE_SHA256, kInput,
      base::BindOnce(
          [](bool* ran_flag, net::SSLPrivateKey::SignCallback inner_callback,
             net::Error error, const std::vector<uint8_t>& signature) {
            *ran_flag = true;
            std::move(inner_callback).Run(error, signature);
          },
          &callback_ran_synchronously, sign_future.GetCallback()));

  // net::SSLPrivateKey::Sign() must not invoke SignCallback synchronously.
  EXPECT_FALSE(callback_ran_synchronously);

  EXPECT_EQ(sign_future.Get<0>(), net::ERR_SSL_CLIENT_AUTH_CERT_NO_PRIVATE_KEY);
  EXPECT_THAT(sign_future.Get<1>(), IsEmpty());
}

TEST_F(SSLPrivateKeyProxyTest, SignWithNullRemote) {
  auto proxy = base::MakeRefCounted<SSLPrivateKeyProxy>(
      "TestProvider", std::vector<uint16_t>{SSL_SIGN_RSA_PSS_RSAE_SHA256},
      mojo::NullRemote());

  const std::vector<uint8_t> kInput = {1, 2, 3};
  base::test::TestFuture<net::Error, const std::vector<uint8_t>&> sign_future;
  proxy->Sign(SSL_SIGN_RSA_PSS_RSAE_SHA256, kInput, sign_future.GetCallback());

  EXPECT_EQ(sign_future.Get<0>(), net::ERR_SSL_CLIENT_AUTH_CERT_NO_PRIVATE_KEY);
  EXPECT_THAT(sign_future.Get<1>(), IsEmpty());
}

TEST_F(SSLPrivateKeyProxyTest, SignDisconnectedWhileInFlight) {
  DeferredMojoSSLPrivateKey impl;
  auto proxy = base::MakeRefCounted<SSLPrivateKeyProxy>(
      "TestProvider", std::vector<uint16_t>{SSL_SIGN_RSA_PSS_RSAE_SHA256},
      impl.BindNewPipeAndPassRemote());

  const std::vector<uint8_t> kInput = {5, 6, 7, 8};
  base::test::TestFuture<net::Error, const std::vector<uint8_t>&> sign_future;
  proxy->Sign(SSL_SIGN_RSA_PSS_RSAE_SHA256, kInput, sign_future.GetCallback());

  // Wait until the Mojo receiver has received the Sign() call so it is
  // guaranteed to be in flight.
  impl.WaitForSignRequest();
  EXPECT_FALSE(sign_future.IsReady());

  // Close the receiver pipe without replying; SignCallback must resolve with
  // ERR_SSL_CLIENT_AUTH_CERT_NO_PRIVATE_KEY rather than hanging.
  impl.ClosePipe();

  EXPECT_EQ(sign_future.Get<0>(), net::ERR_SSL_CLIENT_AUTH_CERT_NO_PRIVATE_KEY);
  EXPECT_THAT(sign_future.Get<1>(), IsEmpty());
}

TEST_F(SSLPrivateKeyProxyTest,
       SignDisconnectedWhileInFlightAfterExternalRefDropped) {
  DeferredMojoSSLPrivateKey impl;
  auto proxy = base::MakeRefCounted<SSLPrivateKeyProxy>(
      "TestProvider", std::vector<uint16_t>{SSL_SIGN_RSA_PSS_RSAE_SHA256},
      impl.BindNewPipeAndPassRemote());

  const std::vector<uint8_t> kInput = {5, 6, 7, 8};
  base::test::TestFuture<net::Error, const std::vector<uint8_t>&> sign_future;
  proxy->Sign(SSL_SIGN_RSA_PSS_RSAE_SHA256, kInput, sign_future.GetCallback());
  proxy.reset();

  impl.WaitForSignRequest();
  impl.ClosePipe();

  EXPECT_EQ(sign_future.Get<0>(), net::ERR_SSL_CLIENT_AUTH_CERT_NO_PRIVATE_KEY);
  EXPECT_THAT(sign_future.Get<1>(), IsEmpty());
}

}  // namespace
}  // namespace network
