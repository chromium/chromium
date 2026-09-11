// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/remote_client_cert_store.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/client_cert_identity.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_private_key.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "remoting/host/mojom/remoting_host.mojom.h"
#include "remoting/host/remote_ssl_private_key.h"
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

  void Sign(uint16_t algorithm,
            const std::vector<uint8_t>& input,
            SignCallback callback) override {
    std::move(callback).Run(static_cast<int32_t>(net::OK), kTestSignature);
  }
};

class FakeCertificateBroker : public mojom::CertificateBroker {
 public:
  FakeCertificateBroker() = default;
  ~FakeCertificateBroker() override = default;

  mojo::PendingAssociatedRemote<mojom::CertificateBroker> BindNewEndpoint() {
    receiver_.reset();
    mojo::AssociatedRemote<mojom::CertificateBroker> remote;
    receiver_.Bind(remote.BindNewEndpointAndPassDedicatedReceiver());
    return remote.Unbind();
  }

  void GetCertificates(GetCertificatesCallback callback) override {
    get_certificates_called_ = true;
    std::vector<mojom::ClientCertificateDetailsPtr> certs;
    for (const auto& info : cert_infos_) {
      auto details = mojom::ClientCertificateDetails::New();
      details->certificate = info.cert;
      details->provider_name = info.provider_name;
      details->algorithm_preferences = info.algorithm_preferences;
      auto fake_key = std::make_unique<FakeSSLPrivateKey>();
      mojo::PendingRemote<network::mojom::SSLPrivateKey> key_remote;
      mojo::MakeSelfOwnedReceiver(std::move(fake_key),
                                  key_remote.InitWithNewPipeAndPassReceiver());
      details->private_key = std::move(key_remote);
      certs.push_back(std::move(details));
    }
    std::move(callback).Run(std::move(certs));
  }

  void AddCertificate(scoped_refptr<net::X509Certificate> cert,
                      const std::string& provider_name,
                      const std::vector<uint16_t>& algorithm_preferences) {
    cert_infos_.push_back(
        {std::move(cert), provider_name, algorithm_preferences});
  }

  bool get_certificates_called() const { return get_certificates_called_; }

 private:
  struct CertInfo {
    scoped_refptr<net::X509Certificate> cert;
    std::string provider_name;
    std::vector<uint16_t> algorithm_preferences;
  };

  mojo::AssociatedReceiver<mojom::CertificateBroker> receiver_{this};
  std::vector<CertInfo> cert_infos_;
  bool get_certificates_called_ = false;
};

}  // namespace

class RemoteClientCertStoreTest : public testing::Test {
 public:
  RemoteClientCertStoreTest() = default;
  ~RemoteClientCertStoreTest() override = default;

  void SetUp() override {
    test_cert_ =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
    ASSERT_TRUE(test_cert_);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<net::X509Certificate> test_cert_;
  FakeCertificateBroker broker_;
};

TEST_F(RemoteClientCertStoreTest, GetClientCertsSuccess) {
  broker_.AddCertificate(test_cert_, "TestProvider", {kTestAlgorithm});

  RemoteClientCertStore store(broker_.BindNewEndpoint());

  base::test::TestFuture<net::ClientCertIdentityList> future;
  store.GetClientCerts(base::MakeRefCounted<net::SSLCertRequestInfo>(),
                       future.GetCallback());

  net::ClientCertIdentityList result_certs = future.Take();
  EXPECT_TRUE(broker_.get_certificates_called());
  ASSERT_EQ(result_certs.size(), 1u);
  EXPECT_TRUE(
      result_certs[0]->certificate()->EqualsExcludingChain(test_cert_.get()));
}

TEST_F(RemoteClientCertStoreTest, AcquirePrivateKeyAndSign) {
  broker_.AddCertificate(test_cert_, "TestProvider", {kTestAlgorithm});

  RemoteClientCertStore store(broker_.BindNewEndpoint());

  base::test::TestFuture<net::ClientCertIdentityList> certs_future;
  store.GetClientCerts(base::MakeRefCounted<net::SSLCertRequestInfo>(),
                       certs_future.GetCallback());
  net::ClientCertIdentityList result_certs = certs_future.Take();
  ASSERT_EQ(result_certs.size(), 1u);

  base::test::TestFuture<scoped_refptr<net::SSLPrivateKey>> key_future;
  net::ClientCertIdentity::SelfOwningAcquirePrivateKey(
      std::move(result_certs[0]), key_future.GetCallback());
  scoped_refptr<net::SSLPrivateKey> acquired_key = key_future.Take();

  ASSERT_TRUE(acquired_key);
  EXPECT_EQ(acquired_key->GetProviderName(), "TestProvider");
  EXPECT_THAT(acquired_key->GetAlgorithmPreferences(),
              testing::ElementsAre(kTestAlgorithm));

  std::vector<uint8_t> input_data = {'h', 'e', 'l', 'l', 'o'};
  base::test::TestFuture<net::Error, const std::vector<uint8_t>&> sign_future;
  acquired_key->Sign(kTestAlgorithm, input_data, sign_future.GetCallback());

  EXPECT_EQ(sign_future.Get<0>(), net::OK);
  EXPECT_EQ(sign_future.Get<1>(), kTestSignature);
}

TEST_F(RemoteClientCertStoreTest, DisconnectedBrokerReturnsEmpty) {
  mojo::AssociatedRemote<mojom::CertificateBroker> remote;
  auto receiver = remote.BindNewEndpointAndPassDedicatedReceiver();
  // Intentionally drop the receiver so it's disconnected/closed.
  receiver.reset();

  RemoteClientCertStore store(remote.Unbind());

  base::test::TestFuture<net::ClientCertIdentityList> future;
  store.GetClientCerts(base::MakeRefCounted<net::SSLCertRequestInfo>(),
                       future.GetCallback());

  EXPECT_TRUE(future.Get().empty());
}

TEST_F(RemoteClientCertStoreTest, AuthorityFiltering) {
  broker_.AddCertificate(test_cert_, "TestProvider", {kTestAlgorithm});

  RemoteClientCertStore store(broker_.BindNewEndpoint());

  // Request with non-matching authority.
  auto cert_request_info = base::MakeRefCounted<net::SSLCertRequestInfo>();
  cert_request_info->cert_authorities = {"non_matching_authority"};

  base::test::TestFuture<net::ClientCertIdentityList> future;
  store.GetClientCerts(cert_request_info, future.GetCallback());

  EXPECT_TRUE(future.Get().empty());
}

TEST_F(RemoteClientCertStoreTest, InvalidBrokerReturnsEmpty) {
  mojo::PendingAssociatedRemote<mojom::CertificateBroker> invalid_broker;
  RemoteClientCertStore store(std::move(invalid_broker));

  base::test::TestFuture<net::ClientCertIdentityList> future;
  store.GetClientCerts(base::MakeRefCounted<net::SSLCertRequestInfo>(),
                       future.GetCallback());

  EXPECT_TRUE(future.Get().empty());
}

TEST_F(RemoteClientCertStoreTest, ConcurrentRequests) {
  broker_.AddCertificate(test_cert_, "TestProvider", {kTestAlgorithm});

  RemoteClientCertStore store(broker_.BindNewEndpoint());

  base::test::TestFuture<net::ClientCertIdentityList> future1;
  base::test::TestFuture<net::ClientCertIdentityList> future2;

  store.GetClientCerts(base::MakeRefCounted<net::SSLCertRequestInfo>(),
                       future1.GetCallback());
  store.GetClientCerts(base::MakeRefCounted<net::SSLCertRequestInfo>(),
                       future2.GetCallback());

  net::ClientCertIdentityList result_certs1 = future1.Take();
  net::ClientCertIdentityList result_certs2 = future2.Take();

  ASSERT_EQ(result_certs1.size(), 1u);
  ASSERT_EQ(result_certs2.size(), 1u);
  EXPECT_EQ(result_certs1[0]->certificate()->serial_number(),
            test_cert_->serial_number());
  EXPECT_EQ(result_certs2[0]->certificate()->serial_number(),
            test_cert_->serial_number());
}

TEST_F(RemoteClientCertStoreTest, DeleteStoreWhileRequestInFlightDoesNotCrash) {
  broker_.AddCertificate(test_cert_, "TestProvider", {kTestAlgorithm});

  auto store =
      std::make_unique<RemoteClientCertStore>(broker_.BindNewEndpoint());

  base::test::TestFuture<net::ClientCertIdentityList> future;
  store->GetClientCerts(base::MakeRefCounted<net::SSLCertRequestInfo>(),
                        future.GetCallback());

  // Deleting the store while the Mojo response is in flight destroys the
  // AssociatedRemote and cancels the pending response callback cleanly.
  store.reset();
  EXPECT_FALSE(future.IsReady());
}

}  // namespace remoting
