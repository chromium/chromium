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
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/client_cert_identity.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_private_key.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
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
};

TEST_F(RemoteClientCertStoreTest, DefaultConstructorReturnsEmpty) {
  RemoteClientCertStore store;
  base::test::TestFuture<net::ClientCertIdentityList> future;
  store.GetClientCerts(base::MakeRefCounted<net::SSLCertRequestInfo>(),
                       future.GetCallback());
  EXPECT_TRUE(future.Get().empty());
}

TEST_F(RemoteClientCertStoreTest, GetClientCertsSuccessAndAcquireKey) {
  auto fake_key = std::make_unique<FakeSSLPrivateKey>();
  mojo::PendingRemote<network::mojom::SSLPrivateKey> key_remote;
  mojo::MakeSelfOwnedReceiver(std::move(fake_key),
                              key_remote.InitWithNewPipeAndPassReceiver());

  RemoteClientCertStore::CertDetails details;
  details.certificate = test_cert_;
  details.provider_name = "TestProvider";
  details.algorithm_preferences = {kTestAlgorithm};
  details.private_key = std::move(key_remote);

  std::vector<RemoteClientCertStore::CertDetails> certs;
  certs.push_back(std::move(details));

  RemoteClientCertStore store(base::BindRepeating(
      [](std::vector<RemoteClientCertStore::CertDetails>* certs,
         base::OnceCallback<void(
             std::vector<RemoteClientCertStore::CertDetails>)> callback) {
        std::move(callback).Run(std::move(*certs));
      },
      base::Unretained(&certs)));

  base::test::TestFuture<net::ClientCertIdentityList> certs_future;
  store.GetClientCerts(base::MakeRefCounted<net::SSLCertRequestInfo>(),
                       certs_future.GetCallback());

  net::ClientCertIdentityList result_certs = certs_future.Take();
  ASSERT_EQ(result_certs.size(), 1u);
  EXPECT_TRUE(
      result_certs[0]->certificate()->EqualsExcludingChain(test_cert_.get()));

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

TEST_F(RemoteClientCertStoreTest, AuthorityFiltering) {
  auto fake_key = std::make_unique<FakeSSLPrivateKey>();
  mojo::PendingRemote<network::mojom::SSLPrivateKey> key_remote;
  mojo::MakeSelfOwnedReceiver(std::move(fake_key),
                              key_remote.InitWithNewPipeAndPassReceiver());

  RemoteClientCertStore::CertDetails details;
  details.certificate = test_cert_;
  details.provider_name = "TestProvider";
  details.algorithm_preferences = {kTestAlgorithm};
  details.private_key = std::move(key_remote);

  std::vector<RemoteClientCertStore::CertDetails> certs;
  certs.push_back(std::move(details));

  RemoteClientCertStore store(base::BindRepeating(
      [](std::vector<RemoteClientCertStore::CertDetails>* certs,
         base::OnceCallback<void(
             std::vector<RemoteClientCertStore::CertDetails>)> callback) {
        std::move(callback).Run(std::move(*certs));
      },
      base::Unretained(&certs)));

  auto cert_request_info = base::MakeRefCounted<net::SSLCertRequestInfo>();
  cert_request_info->cert_authorities = {"non_matching_authority"};

  base::test::TestFuture<net::ClientCertIdentityList> certs_future;
  store.GetClientCerts(cert_request_info, certs_future.GetCallback());

  EXPECT_TRUE(certs_future.Get().empty());
}

TEST_F(RemoteClientCertStoreTest, ConcurrentGetClientCertsFetchesSequentially) {
  int fetch_count = 0;
  base::OnceCallback<void(std::vector<RemoteClientCertStore::CertDetails>)>
      saved_callback;

  auto get_certs_cb = base::BindRepeating(
      [](int* count,
         base::OnceCallback<void(
             std::vector<RemoteClientCertStore::CertDetails>)>* saved_cb,
         base::OnceCallback<void(
             std::vector<RemoteClientCertStore::CertDetails>)> callback) {
        (*count)++;
        *saved_cb = std::move(callback);
      },
      base::Unretained(&fetch_count), base::Unretained(&saved_callback));

  RemoteClientCertStore store(std::move(get_certs_cb));

  base::test::TestFuture<net::ClientCertIdentityList> future1;
  base::test::TestFuture<net::ClientCertIdentityList> future2;

  store.GetClientCerts(base::MakeRefCounted<net::SSLCertRequestInfo>(),
                       future1.GetCallback());
  store.GetClientCerts(base::MakeRefCounted<net::SSLCertRequestInfo>(),
                       future2.GetCallback());

  EXPECT_EQ(fetch_count, 1);
  EXPECT_FALSE(future1.IsReady());
  EXPECT_FALSE(future2.IsReady());
  EXPECT_TRUE(saved_callback);

  // Fulfill the first request.
  auto fake_key1 = std::make_unique<FakeSSLPrivateKey>();
  mojo::PendingRemote<network::mojom::SSLPrivateKey> key_remote1;
  mojo::MakeSelfOwnedReceiver(std::move(fake_key1),
                              key_remote1.InitWithNewPipeAndPassReceiver());

  RemoteClientCertStore::CertDetails details1;
  details1.certificate = test_cert_;
  details1.provider_name = "TestProvider";
  details1.algorithm_preferences = {kTestAlgorithm};
  details1.private_key = std::move(key_remote1);

  std::vector<RemoteClientCertStore::CertDetails> certs1;
  certs1.push_back(std::move(details1));

  std::move(saved_callback).Run(std::move(certs1));

  net::ClientCertIdentityList result_certs1 = future1.Take();
  ASSERT_EQ(result_certs1.size(), 1u);
  EXPECT_EQ(result_certs1[0]->certificate()->serial_number(),
            test_cert_->serial_number());

  // The second request should have triggered a second fetch.
  EXPECT_EQ(fetch_count, 2);
  EXPECT_FALSE(future2.IsReady());
  EXPECT_TRUE(saved_callback);

  // Fulfill the second request.
  auto fake_key2 = std::make_unique<FakeSSLPrivateKey>();
  mojo::PendingRemote<network::mojom::SSLPrivateKey> key_remote2;
  mojo::MakeSelfOwnedReceiver(std::move(fake_key2),
                              key_remote2.InitWithNewPipeAndPassReceiver());

  RemoteClientCertStore::CertDetails details2;
  details2.certificate = test_cert_;
  details2.provider_name = "TestProvider";
  details2.algorithm_preferences = {kTestAlgorithm};
  details2.private_key = std::move(key_remote2);

  std::vector<RemoteClientCertStore::CertDetails> certs2;
  certs2.push_back(std::move(details2));

  std::move(saved_callback).Run(std::move(certs2));

  net::ClientCertIdentityList result_certs2 = future2.Take();
  ASSERT_EQ(result_certs2.size(), 1u);
  EXPECT_EQ(result_certs2[0]->certificate()->serial_number(),
            test_cert_->serial_number());
}

TEST_F(RemoteClientCertStoreTest,
       DeleteStoreInCallbackWithPendingRequestsDoesNotCrash) {
  base::OnceCallback<void(std::vector<RemoteClientCertStore::CertDetails>)>
      saved_callback;

  auto get_certs_cb = base::BindRepeating(
      [](base::OnceCallback<void(
             std::vector<RemoteClientCertStore::CertDetails>)>* saved_cb,
         base::OnceCallback<void(
             std::vector<RemoteClientCertStore::CertDetails>)> callback) {
        *saved_cb = std::move(callback);
      },
      base::Unretained(&saved_callback));

  auto store = std::make_unique<RemoteClientCertStore>(std::move(get_certs_cb));

  base::test::TestFuture<net::ClientCertIdentityList> future1;
  base::test::TestFuture<net::ClientCertIdentityList> future2;

  store->GetClientCerts(
      base::MakeRefCounted<net::SSLCertRequestInfo>(),
      base::BindOnce(
          [](std::unique_ptr<RemoteClientCertStore>* store_ptr,
             base::OnceCallback<void(net::ClientCertIdentityList)> callback,
             net::ClientCertIdentityList certs) {
            store_ptr->reset();
            std::move(callback).Run(std::move(certs));
          },
          base::Unretained(&store), future1.GetCallback()));

  store->GetClientCerts(base::MakeRefCounted<net::SSLCertRequestInfo>(),
                        future2.GetCallback());

  EXPECT_TRUE(saved_callback);

  auto fake_key = std::make_unique<FakeSSLPrivateKey>();
  mojo::PendingRemote<network::mojom::SSLPrivateKey> key_remote;
  mojo::MakeSelfOwnedReceiver(std::move(fake_key),
                              key_remote.InitWithNewPipeAndPassReceiver());

  RemoteClientCertStore::CertDetails details;
  details.certificate = test_cert_;
  details.provider_name = "TestProvider";
  details.algorithm_preferences = {kTestAlgorithm};
  details.private_key = std::move(key_remote);

  std::vector<RemoteClientCertStore::CertDetails> certs;
  certs.push_back(std::move(details));

  // Running the callback deletes the store during future1's callback.
  std::move(saved_callback).Run(std::move(certs));

  EXPECT_EQ(store, nullptr);
  net::ClientCertIdentityList result_certs1 = future1.Take();
  ASSERT_EQ(result_certs1.size(), 1u);
}

TEST_F(RemoteClientCertStoreTest,
       DeleteStoreInSynchronousSubsequentCallbackDoesNotCrash) {
  base::OnceCallback<void(std::vector<RemoteClientCertStore::CertDetails>)>
      saved_first_callback;
  bool is_first_fetch = true;

  auto get_certs_cb = base::BindRepeating(
      [](bool* first_fetch,
         base::OnceCallback<void(
             std::vector<RemoteClientCertStore::CertDetails>)>* saved_cb,
         base::OnceCallback<void(
             std::vector<RemoteClientCertStore::CertDetails>)> callback) {
        if (*first_fetch) {
          *first_fetch = false;
          *saved_cb = std::move(callback);
        } else {
          // Synchronously fulfill subsequent requests.
          std::move(callback).Run({});
        }
      },
      base::Unretained(&is_first_fetch),
      base::Unretained(&saved_first_callback));

  auto store = std::make_unique<RemoteClientCertStore>(std::move(get_certs_cb));

  base::test::TestFuture<net::ClientCertIdentityList> future1;
  base::test::TestFuture<net::ClientCertIdentityList> future2;
  base::test::TestFuture<net::ClientCertIdentityList> future3;

  store->GetClientCerts(base::MakeRefCounted<net::SSLCertRequestInfo>(),
                        future1.GetCallback());
  store->GetClientCerts(
      base::MakeRefCounted<net::SSLCertRequestInfo>(),
      base::BindOnce(
          [](std::unique_ptr<RemoteClientCertStore>* store_ptr,
             base::OnceCallback<void(net::ClientCertIdentityList)> callback,
             net::ClientCertIdentityList certs) {
            // Delete store synchronously during future2.
            store_ptr->reset();
            std::move(callback).Run(std::move(certs));
          },
          base::Unretained(&store), future2.GetCallback()));
  store->GetClientCerts(base::MakeRefCounted<net::SSLCertRequestInfo>(),
                        future3.GetCallback());

  EXPECT_TRUE(saved_first_callback);

  // Complete first request asynchronously.
  std::move(saved_first_callback).Run({});

  EXPECT_EQ(store, nullptr);
  EXPECT_TRUE(future1.Take().empty());
  EXPECT_TRUE(future2.Take().empty());
  EXPECT_FALSE(future3.IsReady());
}

}  // namespace remoting
