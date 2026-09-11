// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/certificate_broker_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/client_cert_identity.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_private_key.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "remoting/host/mojom/remoting_host.mojom.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

constexpr uint16_t kTestAlgorithm = 0x0403;  // ecdsa_secp256r1_sha256
const std::vector<uint8_t> kTestSignature = {1, 2, 3, 4, 5};

class FakeSSLPrivateKey : public net::SSLPrivateKey {
 public:
  FakeSSLPrivateKey() = default;

  std::string GetProviderName() override { return "TestProvider"; }

  std::vector<uint16_t> GetAlgorithmPreferences() override {
    return {kTestAlgorithm};
  }

  void Sign(uint16_t algorithm,
            base::span<const uint8_t> input,
            SignCallback callback) override {
    last_algorithm_ = algorithm;
    last_input_.assign(input.begin(), input.end());
    std::move(callback).Run(net::OK, kTestSignature);
  }

  uint16_t last_algorithm() const { return last_algorithm_; }
  const std::vector<uint8_t>& last_input() const { return last_input_; }

 private:
  ~FakeSSLPrivateKey() override = default;

  uint16_t last_algorithm_ = 0;
  std::vector<uint8_t> last_input_;
};

class FakeClientCertIdentity : public net::ClientCertIdentity {
 public:
  FakeClientCertIdentity(scoped_refptr<net::X509Certificate> cert,
                         scoped_refptr<net::SSLPrivateKey> key)
      : net::ClientCertIdentity(std::move(cert)), key_(std::move(key)) {}
  ~FakeClientCertIdentity() override = default;

  void AcquirePrivateKey(
      base::OnceCallback<void(scoped_refptr<net::SSLPrivateKey>)> callback)
      override {
    std::move(callback).Run(key_);
  }

 private:
  scoped_refptr<net::SSLPrivateKey> key_;
};

class FakeClientCertStore : public net::ClientCertStore {
 public:
  FakeClientCertStore() = default;
  ~FakeClientCertStore() override = default;

  void AddIdentity(std::unique_ptr<net::ClientCertIdentity> identity) {
    identities_.push_back(std::move(identity));
  }

  void GetClientCerts(
      scoped_refptr<const net::SSLCertRequestInfo> cert_request_info,
      ClientCertListCallback callback) override {
    std::move(callback).Run(std::move(identities_));
  }

 private:
  net::ClientCertIdentityList identities_;
};

}  // namespace

class CertificateBrokerImplTest : public testing::Test {
 public:
  CertificateBrokerImplTest() = default;
  ~CertificateBrokerImplTest() override = default;

  void SetUp() override {
    test_cert_ =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
    ASSERT_TRUE(test_cert_);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<net::X509Certificate> test_cert_;
};

TEST_F(CertificateBrokerImplTest, GetCertificatesSuccess) {
  auto fake_key = base::MakeRefCounted<FakeSSLPrivateKey>();
  FakeSSLPrivateKey* raw_key = fake_key.get();

  auto fake_store = std::make_unique<FakeClientCertStore>();
  fake_store->AddIdentity(
      std::make_unique<FakeClientCertIdentity>(test_cert_, fake_key));

  CertificateBrokerImpl broker(base::BindRepeating(
      [](std::unique_ptr<FakeClientCertStore>* store)
          -> std::unique_ptr<net::ClientCertStore> {
        return std::move(*store);
      },
      base::Unretained(&fake_store)));

  base::test::TestFuture<std::vector<mojom::ClientCertificateDetailsPtr>>
      future;
  broker.GetCertificates(future.GetCallback());
  std::vector<mojom::ClientCertificateDetailsPtr> results = future.Take();

  ASSERT_EQ(results.size(), 1u);
  ASSERT_TRUE(results[0]->certificate);
  EXPECT_TRUE(results[0]->certificate->EqualsExcludingChain(test_cert_.get()));
  EXPECT_EQ(results[0]->provider_name, "TestProvider");
  EXPECT_THAT(results[0]->algorithm_preferences,
              testing::ElementsAre(kTestAlgorithm));
  ASSERT_TRUE(results[0]->private_key.is_valid());

  mojo::Remote<network::mojom::SSLPrivateKey> private_key_remote(
      std::move(results[0]->private_key));
  std::vector<uint8_t> input = {'d', 'a', 't', 'a'};
  base::test::TestFuture<int32_t, const std::vector<uint8_t>&> sign_future;
  private_key_remote->Sign(kTestAlgorithm, input, sign_future.GetCallback());

  EXPECT_EQ(sign_future.Get<0>(), net::OK);
  EXPECT_EQ(sign_future.Get<1>(), kTestSignature);
  EXPECT_EQ(raw_key->last_algorithm(), kTestAlgorithm);
  EXPECT_EQ(raw_key->last_input(), input);
}

TEST_F(CertificateBrokerImplTest, GetCertificatesEmptyStore) {
  auto fake_store = std::make_unique<FakeClientCertStore>();

  CertificateBrokerImpl broker(base::BindRepeating(
      [](std::unique_ptr<FakeClientCertStore>* store)
          -> std::unique_ptr<net::ClientCertStore> {
        return std::move(*store);
      },
      base::Unretained(&fake_store)));

  base::test::TestFuture<std::vector<mojom::ClientCertificateDetailsPtr>>
      future;
  broker.GetCertificates(future.GetCallback());
  std::vector<mojom::ClientCertificateDetailsPtr> results = future.Take();

  EXPECT_TRUE(results.empty());
}

TEST_F(CertificateBrokerImplTest, GetCertificatesNullStore) {
  CertificateBrokerImpl broker(base::BindRepeating(
      []() -> std::unique_ptr<net::ClientCertStore> { return nullptr; }));

  base::test::TestFuture<std::vector<mojom::ClientCertificateDetailsPtr>>
      future;
  broker.GetCertificates(future.GetCallback());
  std::vector<mojom::ClientCertificateDetailsPtr> results = future.Take();

  EXPECT_TRUE(results.empty());
}

TEST_F(CertificateBrokerImplTest, GetCertificatesAcquirePrivateKeyFailure) {
  auto fake_store = std::make_unique<FakeClientCertStore>();
  fake_store->AddIdentity(
      std::make_unique<FakeClientCertIdentity>(test_cert_, nullptr));

  CertificateBrokerImpl broker(base::BindRepeating(
      [](std::unique_ptr<FakeClientCertStore>* store)
          -> std::unique_ptr<net::ClientCertStore> {
        return std::move(*store);
      },
      base::Unretained(&fake_store)));

  base::test::TestFuture<std::vector<mojom::ClientCertificateDetailsPtr>>
      future;
  broker.GetCertificates(future.GetCallback());
  std::vector<mojom::ClientCertificateDetailsPtr> results = future.Take();

  EXPECT_TRUE(results.empty());
}

}  // namespace remoting
