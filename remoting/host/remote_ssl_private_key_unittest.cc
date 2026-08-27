// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/remote_ssl_private_key.h"

#include <memory>
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

namespace remoting {

namespace {

constexpr uint16_t kTestAlgorithm = 0x0403;  // ecdsa_secp256r1_sha256
const std::vector<uint8_t> kTestSignature = {1, 2, 3, 4, 5};

class FakeSSLPrivateKey : public network::mojom::SSLPrivateKey {
 public:
  FakeSSLPrivateKey() = default;
  ~FakeSSLPrivateKey() override = default;

  mojo::PendingRemote<network::mojom::SSLPrivateKey>
  BindNewPipeAndPassRemote() {
    receiver_.reset();
    return receiver_.BindNewPipeAndPassRemote();
  }

  void Sign(uint16_t algorithm,
            const std::vector<uint8_t>& input,
            SignCallback callback) override {
    last_algorithm_ = algorithm;
    last_input_ = input;
    if (drop_callback_) {
      // Intentionally drop the callback without running it.
      return;
    }
    if (fail_sign_) {
      std::move(callback).Run(static_cast<int32_t>(net::ERR_FAILED), {});
    } else {
      std::move(callback).Run(static_cast<int32_t>(net::OK), kTestSignature);
    }
  }

  void CloseReceiver() { receiver_.reset(); }

  void set_drop_callback(bool drop_callback) { drop_callback_ = drop_callback; }
  void set_fail_sign(bool fail_sign) { fail_sign_ = fail_sign; }
  uint16_t last_algorithm() const { return last_algorithm_; }
  const std::vector<uint8_t>& last_input() const { return last_input_; }

 private:
  mojo::Receiver<network::mojom::SSLPrivateKey> receiver_{this};
  bool drop_callback_ = false;
  bool fail_sign_ = false;
  uint16_t last_algorithm_ = 0;
  std::vector<uint8_t> last_input_;
};

}  // namespace

class RemoteSSLPrivateKeyTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  FakeSSLPrivateKey fake_key_;
};

TEST_F(RemoteSSLPrivateKeyTest, GettersReturnConfiguredValues) {
  auto ssl_key = base::MakeRefCounted<RemoteSSLPrivateKey>(
      "TestProvider", std::vector<uint16_t>{kTestAlgorithm},
      fake_key_.BindNewPipeAndPassRemote());

  EXPECT_EQ(ssl_key->GetProviderName(), "TestProvider");
  EXPECT_THAT(ssl_key->GetAlgorithmPreferences(),
              testing::ElementsAre(kTestAlgorithm));
}

TEST_F(RemoteSSLPrivateKeyTest, SignSuccess) {
  auto ssl_key = base::MakeRefCounted<RemoteSSLPrivateKey>(
      "TestProvider", std::vector<uint16_t>{kTestAlgorithm},
      fake_key_.BindNewPipeAndPassRemote());

  std::vector<uint8_t> input_data = {'h', 'e', 'l', 'l', 'o'};
  base::test::TestFuture<net::Error, const std::vector<uint8_t>&> future;
  ssl_key->Sign(kTestAlgorithm, input_data, future.GetCallback());

  EXPECT_EQ(future.Get<0>(), net::OK);
  EXPECT_EQ(future.Get<1>(), kTestSignature);
  EXPECT_EQ(fake_key_.last_algorithm(), kTestAlgorithm);
  EXPECT_EQ(fake_key_.last_input(), input_data);
}

TEST_F(RemoteSSLPrivateKeyTest, SignFailsWhenRemoteFails) {
  fake_key_.set_fail_sign(true);
  auto ssl_key = base::MakeRefCounted<RemoteSSLPrivateKey>(
      "TestProvider", std::vector<uint16_t>{kTestAlgorithm},
      fake_key_.BindNewPipeAndPassRemote());

  std::vector<uint8_t> input_data = {'t', 'e', 's', 't'};
  base::test::TestFuture<net::Error, const std::vector<uint8_t>&> future;
  ssl_key->Sign(kTestAlgorithm, input_data, future.GetCallback());

  EXPECT_EQ(future.Get<0>(), net::ERR_FAILED);
  EXPECT_TRUE(future.Get<1>().empty());
}

TEST_F(RemoteSSLPrivateKeyTest, SignRejectsOversizedInput) {
  auto ssl_key = base::MakeRefCounted<RemoteSSLPrivateKey>(
      "TestProvider", std::vector<uint16_t>{kTestAlgorithm},
      fake_key_.BindNewPipeAndPassRemote());

  std::vector<uint8_t> oversized_input(
      RemoteSSLPrivateKey::kMaxSignatureInputSize + 1, 0xAA);
  base::test::TestFuture<net::Error, const std::vector<uint8_t>&> future;
  ssl_key->Sign(kTestAlgorithm, oversized_input, future.GetCallback());

  EXPECT_EQ(future.Get<0>(), net::ERR_FAILED);
  EXPECT_TRUE(future.Get<1>().empty());
}

TEST_F(RemoteSSLPrivateKeyTest, SignFailsWhenDisconnectedBeforeSign) {
  auto remote = fake_key_.BindNewPipeAndPassRemote();
  fake_key_.CloseReceiver();

  auto ssl_key = base::MakeRefCounted<RemoteSSLPrivateKey>(
      "TestProvider", std::vector<uint16_t>{kTestAlgorithm}, std::move(remote));

  task_environment_.RunUntilIdle();

  std::vector<uint8_t> input_data = {'d', 'a', 't', 'a'};
  base::test::TestFuture<net::Error, const std::vector<uint8_t>&> future;
  ssl_key->Sign(kTestAlgorithm, input_data, future.GetCallback());

  EXPECT_EQ(future.Get<0>(), net::ERR_FAILED);
  EXPECT_TRUE(future.Get<1>().empty());
}

TEST_F(RemoteSSLPrivateKeyTest, DisconnectDuringSignInFlightFailsSign) {
  auto ssl_key = base::MakeRefCounted<RemoteSSLPrivateKey>(
      "TestProvider", std::vector<uint16_t>{kTestAlgorithm},
      fake_key_.BindNewPipeAndPassRemote());

  fake_key_.set_drop_callback(true);

  std::vector<uint8_t> input_data = {'p', 'e', 'n', 'd', 'i', 'n', 'g'};
  base::test::TestFuture<net::Error, const std::vector<uint8_t>&> future;
  ssl_key->Sign(kTestAlgorithm, input_data, future.GetCallback());

  // Close the receiver while the sign request is in flight.
  fake_key_.CloseReceiver();

  EXPECT_EQ(future.Get<0>(), net::ERR_FAILED);
  EXPECT_TRUE(future.Get<1>().empty());
}

TEST_F(RemoteSSLPrivateKeyTest, DestroyObjectDuringSignInFlightFailsSign) {
  auto ssl_key = base::MakeRefCounted<RemoteSSLPrivateKey>(
      "TestProvider", std::vector<uint16_t>{kTestAlgorithm},
      fake_key_.BindNewPipeAndPassRemote());

  fake_key_.set_drop_callback(true);

  std::vector<uint8_t> input_data = {'p', 'e', 'n', 'd', 'i', 'n', 'g'};
  base::test::TestFuture<net::Error, const std::vector<uint8_t>&> future;
  ssl_key->Sign(kTestAlgorithm, input_data, future.GetCallback());

  // Destroy the RemoteSSLPrivateKey instance while the sign request is in
  // flight.
  ssl_key.reset();

  EXPECT_EQ(future.Get<0>(), net::ERR_FAILED);
  EXPECT_TRUE(future.Get<1>().empty());
}

}  // namespace remoting
