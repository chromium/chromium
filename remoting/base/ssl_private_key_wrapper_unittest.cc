// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/ssl_private_key_wrapper.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "net/ssl/ssl_private_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

constexpr uint16_t kTestAlgorithm = 0x0403;  // ecdsa_secp256r1_sha256
const std::vector<uint8_t> kTestSignature = {1, 2, 3, 4, 5};

class MockSSLPrivateKey : public net::SSLPrivateKey {
 public:
  MockSSLPrivateKey() = default;

  std::string GetProviderName() override { return "MockProvider"; }

  std::vector<uint16_t> GetAlgorithmPreferences() override {
    return {kTestAlgorithm};
  }

  void Sign(uint16_t algorithm,
            base::span<const uint8_t> input,
            SignCallback callback) override {
    sign_called_ = true;
    last_algorithm_ = algorithm;
    last_input_.assign(input.begin(), input.end());
    if (fail_) {
      std::move(callback).Run(net::ERR_FAILED, {});
    } else {
      std::move(callback).Run(net::OK, kTestSignature);
    }
  }

  void set_fail(bool fail) { fail_ = fail; }
  bool sign_called() const { return sign_called_; }
  uint16_t last_algorithm() const { return last_algorithm_; }
  const std::vector<uint8_t>& last_input() const { return last_input_; }

 private:
  ~MockSSLPrivateKey() override = default;

  bool fail_ = false;
  bool sign_called_ = false;
  uint16_t last_algorithm_ = 0;
  std::vector<uint8_t> last_input_;
};

}  // namespace

class SSLPrivateKeyWrapperTest : public testing::Test {
 public:
  SSLPrivateKeyWrapperTest() = default;
  ~SSLPrivateKeyWrapperTest() override = default;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(SSLPrivateKeyWrapperTest, SignSuccess) {
  auto mock_key = base::MakeRefCounted<MockSSLPrivateKey>();
  SSLPrivateKeyWrapper wrapper(mock_key);

  mojo::Remote<network::mojom::SSLPrivateKey> remote;
  mojo::Receiver<network::mojom::SSLPrivateKey> receiver(
      &wrapper, remote.BindNewPipeAndPassReceiver());

  std::vector<uint8_t> input_data = {'t', 'e', 's', 't'};
  base::RunLoop run_loop;
  int32_t sign_net_error = static_cast<int32_t>(net::ERR_FAILED);
  std::vector<uint8_t> sign_signature;

  remote->Sign(kTestAlgorithm, input_data,
               base::BindOnce(
                   [](base::RunLoop* loop, int32_t* out_err,
                      std::vector<uint8_t>* out_sig, int32_t net_error,
                      const std::vector<uint8_t>& signature) {
                     *out_err = net_error;
                     *out_sig = signature;
                     loop->Quit();
                   },
                   &run_loop, &sign_net_error, &sign_signature));
  run_loop.Run();

  EXPECT_EQ(sign_net_error, static_cast<int32_t>(net::OK));
  EXPECT_EQ(sign_signature, kTestSignature);
  EXPECT_TRUE(mock_key->sign_called());
  EXPECT_EQ(mock_key->last_algorithm(), kTestAlgorithm);
  EXPECT_EQ(mock_key->last_input(), input_data);
}

TEST_F(SSLPrivateKeyWrapperTest, SignFailure) {
  auto mock_key = base::MakeRefCounted<MockSSLPrivateKey>();
  mock_key->set_fail(true);
  SSLPrivateKeyWrapper wrapper(mock_key);

  mojo::Remote<network::mojom::SSLPrivateKey> remote;
  mojo::Receiver<network::mojom::SSLPrivateKey> receiver(
      &wrapper, remote.BindNewPipeAndPassReceiver());

  std::vector<uint8_t> input_data = {'t', 'e', 's', 't'};
  base::RunLoop run_loop;
  int32_t sign_net_error = static_cast<int32_t>(net::OK);
  std::vector<uint8_t> sign_signature;

  remote->Sign(kTestAlgorithm, input_data,
               base::BindOnce(
                   [](base::RunLoop* loop, int32_t* out_err,
                      std::vector<uint8_t>* out_sig, int32_t net_error,
                      const std::vector<uint8_t>& signature) {
                     *out_err = net_error;
                     *out_sig = signature;
                     loop->Quit();
                   },
                   &run_loop, &sign_net_error, &sign_signature));
  run_loop.Run();

  EXPECT_EQ(sign_net_error, static_cast<int32_t>(net::ERR_FAILED));
  EXPECT_TRUE(sign_signature.empty());
  EXPECT_TRUE(mock_key->sign_called());
}

TEST_F(SSLPrivateKeyWrapperTest, SignRejectsOversizedInput) {
  auto mock_key = base::MakeRefCounted<MockSSLPrivateKey>();
  SSLPrivateKeyWrapper wrapper(mock_key);

  mojo::Remote<network::mojom::SSLPrivateKey> remote;
  mojo::Receiver<network::mojom::SSLPrivateKey> receiver(
      &wrapper, remote.BindNewPipeAndPassReceiver());

  std::vector<uint8_t> oversized_input(70 * 1024, 0xBB);  // 70 KB > 64 KB limit
  base::RunLoop run_loop;
  int32_t sign_net_error = static_cast<int32_t>(net::OK);
  std::vector<uint8_t> sign_signature;

  remote->Sign(kTestAlgorithm, oversized_input,
               base::BindOnce(
                   [](base::RunLoop* loop, int32_t* out_err,
                      std::vector<uint8_t>* out_sig, int32_t net_error,
                      const std::vector<uint8_t>& signature) {
                     *out_err = net_error;
                     *out_sig = signature;
                     loop->Quit();
                   },
                   &run_loop, &sign_net_error, &sign_signature));
  run_loop.Run();

  EXPECT_EQ(sign_net_error, static_cast<int32_t>(net::ERR_FAILED));
  EXPECT_TRUE(sign_signature.empty());
  EXPECT_FALSE(mock_key->sign_called());
}

}  // namespace remoting
