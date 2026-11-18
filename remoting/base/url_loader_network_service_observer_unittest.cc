// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/url_loader_network_service_observer.h"

#include <stdint.h>

#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/sequence_bound.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/client_cert_identity.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_private_key.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

constexpr char kTestProviderName[] = "TestProvider";
constexpr uint16_t kTestAlgorithm = 0x0403;  // ecdsa_secp256r1_sha256

// State that is shared between the test and the fake objects, which live on
// the sequence of the observer.
struct FakeState : public base::RefCountedThreadSafe<FakeState> {
  void ExpectOnObserverSequence() const {
    EXPECT_TRUE(observer_task_runner->RunsTasksInCurrentSequence());
  }

  // Configuration. Must not be changed once the observer is in use.
  scoped_refptr<base::SequencedTaskRunner> observer_task_runner;
  scoped_refptr<net::X509Certificate> cert;
  bool create_null_store = false;
  // Only used by the tests in which the observer lives on the main sequence.
  bool hold_client_certs_requests = false;

  // Written on the observer's sequence.
  std::atomic<int> create_store_calls = 0;
  std::atomic<int> stores_destroyed = 0;
  std::atomic<int> keys_created = 0;
  std::atomic<int> keys_destroyed = 0;
  // The requests that are held when `hold_client_certs_requests` is set. Only
  // used by the tests in which the observer lives on the main sequence.
  std::vector<base::OnceClosure> held_requests;

 private:
  friend class base::RefCountedThreadSafe<FakeState>;
  ~FakeState() = default;
};

class FakeSSLPrivateKey : public net::SSLPrivateKey {
 public:
  explicit FakeSSLPrivateKey(scoped_refptr<FakeState> state)
      : state_(std::move(state)) {
    state_->ExpectOnObserverSequence();
    ++state_->keys_created;
  }

  std::string GetProviderName() override {
    state_->ExpectOnObserverSequence();
    return kTestProviderName;
  }

  std::vector<uint16_t> GetAlgorithmPreferences() override {
    state_->ExpectOnObserverSequence();
    return {kTestAlgorithm};
  }

  // Returns `input` as the signature, so that tests can verify that it has been
  // forwarded.
  void Sign(uint16_t algorithm,
            base::span<const uint8_t> input,
            SignCallback callback) override {
    state_->ExpectOnObserverSequence();
    if (algorithm != kTestAlgorithm) {
      std::move(callback).Run(net::ERR_FAILED, {});
      return;
    }
    std::move(callback).Run(net::OK,
                            std::vector<uint8_t>(input.begin(), input.end()));
  }

 private:
  ~FakeSSLPrivateKey() override {
    state_->ExpectOnObserverSequence();
    ++state_->keys_destroyed;
  }

  scoped_refptr<FakeState> state_;
};

class FakeClientCertIdentity : public net::ClientCertIdentity {
 public:
  explicit FakeClientCertIdentity(scoped_refptr<FakeState> state)
      : net::ClientCertIdentity(state->cert), state_(std::move(state)) {
    state_->ExpectOnObserverSequence();
  }

  ~FakeClientCertIdentity() override { state_->ExpectOnObserverSequence(); }

  void AcquirePrivateKey(
      base::OnceCallback<void(scoped_refptr<net::SSLPrivateKey>)>
          private_key_callback) override {
    state_->ExpectOnObserverSequence();
    std::move(private_key_callback)
        .Run(base::MakeRefCounted<FakeSSLPrivateKey>(state_));
  }

 private:
  scoped_refptr<FakeState> state_;
};

class FakeClientCertStore : public net::ClientCertStore {
 public:
  explicit FakeClientCertStore(scoped_refptr<FakeState> state)
      : state_(std::move(state)) {
    state_->ExpectOnObserverSequence();
  }

  ~FakeClientCertStore() override {
    state_->ExpectOnObserverSequence();
    // A store must not be destroyed by the callback that it is running.
    EXPECT_FALSE(running_callback_);
    ++state_->stores_destroyed;
  }

  void GetClientCerts(
      scoped_refptr<const net::SSLCertRequestInfo> cert_request_info,
      ClientCertListCallback callback) override {
    state_->ExpectOnObserverSequence();
    // Like ash::ClientCertStoreKcer, each store only handles one request.
    EXPECT_FALSE(used_);
    used_ = true;

    callback_ = std::move(callback);
    if (state_->hold_client_certs_requests) {
      // Like a real store, the callback is dropped if the store is destroyed.
      state_->held_requests.push_back(base::BindOnce(
          &FakeClientCertStore::ReturnClientCerts, weak_factory_.GetWeakPtr()));
      return;
    }
    ReturnClientCerts();
  }

 private:
  void ReturnClientCerts() {
    state_->ExpectOnObserverSequence();
    base::AutoReset<bool> running_callback(&running_callback_, true);
    net::ClientCertIdentityList identities;
    if (state_->cert) {
      identities.push_back(std::make_unique<FakeClientCertIdentity>(state_));
    }
    std::move(callback_).Run(std::move(identities));
  }

  scoped_refptr<FakeState> state_;
  bool used_ = false;
  bool running_callback_ = false;
  ClientCertListCallback callback_;
  base::WeakPtrFactory<FakeClientCertStore> weak_factory_{this};
};

std::unique_ptr<net::ClientCertStore> CreateFakeClientCertStore(
    scoped_refptr<FakeState> state) {
  state->ExpectOnObserverSequence();
  ++state->create_store_calls;
  if (state->create_null_store) {
    return nullptr;
  }
  return std::make_unique<FakeClientCertStore>(std::move(state));
}

// Receives the response to a certificate request, like the network service
// does.
class FakeClientCertificateResponder
    : public network::mojom::ClientCertificateResponder {
 public:
  enum class Response {
    kContinueWithCertificate,
    kContinueWithoutCertificate,
    kCancelRequest,
    kDisconnected,
  };

  FakeClientCertificateResponder() = default;
  ~FakeClientCertificateResponder() override = default;

  mojo::PendingRemote<network::mojom::ClientCertificateResponder>
  BindNewPipeAndPassRemote() {
    auto remote = receiver_.BindNewPipeAndPassRemote();
    // Unretained is safe since `receiver_` is owned by `this`.
    receiver_.set_disconnect_handler(
        base::BindOnce(&FakeClientCertificateResponder::OnDisconnected,
                       base::Unretained(this)));
    return remote;
  }

  Response WaitForResponse() { return response_.Get(); }
  bool HasResponse() const { return response_.IsReady(); }
  void Reset() { receiver_.reset(); }

  const scoped_refptr<net::X509Certificate>& certificate() const {
    return certificate_;
  }
  const std::string& provider_name() const { return provider_name_; }
  const std::vector<uint16_t>& algorithm_preferences() const {
    return algorithm_preferences_;
  }
  mojo::Remote<network::mojom::SSLPrivateKey>& ssl_private_key() {
    return ssl_private_key_;
  }

  // network::mojom::ClientCertificateResponder implementation:
  void ContinueWithCertificate(
      const scoped_refptr<net::X509Certificate>& x509_certificate,
      const std::string& provider_name,
      const std::vector<uint16_t>& algorithm_preferences,
      mojo::PendingRemote<network::mojom::SSLPrivateKey> ssl_private_key)
      override {
    certificate_ = x509_certificate;
    provider_name_ = provider_name;
    algorithm_preferences_ = algorithm_preferences;
    ssl_private_key_.Bind(std::move(ssl_private_key));
    SetResponse(Response::kContinueWithCertificate);
  }

  void ContinueWithoutCertificate() override {
    SetResponse(Response::kContinueWithoutCertificate);
  }

  void CancelRequest() override { SetResponse(Response::kCancelRequest); }

 private:
  void OnDisconnected() {
    // The observer closes the pipe once it has responded.
    if (!response_.IsReady()) {
      SetResponse(Response::kDisconnected);
    }
  }

  void SetResponse(Response response) {
    EXPECT_FALSE(response_.IsReady());
    response_.SetValue(response);
  }

  mojo::Receiver<network::mojom::ClientCertificateResponder> receiver_{this};
  base::test::TestFuture<Response> response_;
  scoped_refptr<net::X509Certificate> certificate_;
  std::string provider_name_;
  std::vector<uint16_t> algorithm_preferences_;
  mojo::Remote<network::mojom::SSLPrivateKey> ssl_private_key_;
};

using Response = FakeClientCertificateResponder::Response;

}  // namespace

class UrlLoaderNetworkServiceObserverTest : public testing::Test {
 public:
  void SetUp() override {
    state_->observer_task_runner =
        base::SequencedTaskRunner::GetCurrentDefault();
    state_->cert =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
    ASSERT_TRUE(state_->cert);
  }

  void TearDown() override {
    observer_remote_.reset();
    observer_.reset();
    if (state_->observer_task_runner->RunsTasksInCurrentSequence()) {
      // A private key is owned by the pipe that was given to the responder,
      // and is destroyed by a task once the responder has closed it.
      EXPECT_TRUE(base::test::RunUntil([&]() {
        return state_->keys_destroyed.load() == state_->keys_created.load();
      }));
    }
  }

 protected:
  UrlLoaderNetworkServiceObserver::CreateClientCertStoreCallback
  CreateStoreCallback() {
    return base::BindRepeating(&CreateFakeClientCertStore, state_);
  }

  // Creates an observer that lives on the main sequence.
  void CreateObserver() {
    observer_ = std::make_unique<UrlLoaderNetworkServiceObserver>(
        CreateStoreCallback());
    observer_->BindReceiver(observer_remote_.BindNewPipeAndPassReceiver());
  }

  void RequestCertificate(FakeClientCertificateResponder& responder) {
    observer_remote_->OnCertificateRequested(
        std::nullopt, base::MakeRefCounted<net::SSLCertRequestInfo>(),
        responder.BindNewPipeAndPassRemote());
  }

  // Verifies that `responder` has received the test certificate, and that its
  // private key signs with the underlying key.
  void ExpectCertificateAndSign(FakeClientCertificateResponder& responder) {
    ASSERT_EQ(responder.WaitForResponse(), Response::kContinueWithCertificate);
    ASSERT_TRUE(responder.certificate());
    EXPECT_TRUE(
        responder.certificate()->EqualsExcludingChain(state_->cert.get()));
    EXPECT_EQ(responder.provider_name(), kTestProviderName);
    EXPECT_EQ(responder.algorithm_preferences(),
              std::vector<uint16_t>{kTestAlgorithm});

    const std::vector<uint8_t> input = {1, 2, 3, 4, 5};
    base::test::TestFuture<int32_t, std::vector<uint8_t>> sign_future;
    responder.ssl_private_key()->Sign(
        kTestAlgorithm, input,
        sign_future.GetCallback<int32_t, const std::vector<uint8_t>&>());
    EXPECT_EQ(sign_future.Get<0>(), net::OK);
    EXPECT_EQ(sign_future.Get<1>(), input);
  }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<FakeState> state_ = base::MakeRefCounted<FakeState>();
  std::unique_ptr<UrlLoaderNetworkServiceObserver> observer_;
  mojo::Remote<network::mojom::URLLoaderNetworkServiceObserver>
      observer_remote_;
};

TEST_F(UrlLoaderNetworkServiceObserverTest, ProvidesCertificateAndKey) {
  CreateObserver();

  FakeClientCertificateResponder responder;
  RequestCertificate(responder);

  ExpectCertificateAndSign(responder);
}

TEST_F(UrlLoaderNetworkServiceObserverTest, NoCertificates) {
  state_->cert = nullptr;
  CreateObserver();

  FakeClientCertificateResponder responder;
  RequestCertificate(responder);

  EXPECT_EQ(responder.WaitForResponse(), Response::kContinueWithoutCertificate);
}

TEST_F(UrlLoaderNetworkServiceObserverTest, NullClientCertStore) {
  state_->create_null_store = true;
  CreateObserver();

  FakeClientCertificateResponder responder_1;
  RequestCertificate(responder_1);
  EXPECT_EQ(responder_1.WaitForResponse(),
            Response::kContinueWithoutCertificate);
  EXPECT_EQ(state_->create_store_calls.load(), 1);

  // The next request tries to create the store again.
  FakeClientCertificateResponder responder_2;
  RequestCertificate(responder_2);
  EXPECT_EQ(responder_2.WaitForResponse(),
            Response::kContinueWithoutCertificate);
  EXPECT_EQ(state_->create_store_calls.load(), 2);
}

TEST_F(UrlLoaderNetworkServiceObserverTest, EachRequestUsesNewClientCertStore) {
  CreateObserver();

  // The fake store verifies that it is only used for one request.
  FakeClientCertificateResponder responder_1;
  RequestCertificate(responder_1);
  EXPECT_EQ(responder_1.WaitForResponse(), Response::kContinueWithCertificate);
  FakeClientCertificateResponder responder_2;
  RequestCertificate(responder_2);
  EXPECT_EQ(responder_2.WaitForResponse(), Response::kContinueWithCertificate);

  EXPECT_EQ(state_->create_store_calls.load(), 2);
  // A store is destroyed once its request has completed.
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return state_->stores_destroyed.load() == 2; }));
}

TEST_F(UrlLoaderNetworkServiceObserverTest, RequestsCompleteInReverseOrder) {
  state_->hold_client_certs_requests = true;
  CreateObserver();

  FakeClientCertificateResponder responder_1;
  FakeClientCertificateResponder responder_2;
  RequestCertificate(responder_1);
  RequestCertificate(responder_2);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return state_->held_requests.size() == 2; }));
  EXPECT_EQ(state_->stores_destroyed.load(), 0);

  std::move(state_->held_requests[1]).Run();
  EXPECT_EQ(responder_2.WaitForResponse(), Response::kContinueWithCertificate);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return state_->stores_destroyed.load() == 1; }));
  EXPECT_FALSE(responder_1.HasResponse());

  std::move(state_->held_requests[0]).Run();
  EXPECT_EQ(responder_1.WaitForResponse(), Response::kContinueWithCertificate);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return state_->stores_destroyed.load() == 2; }));
}

TEST_F(UrlLoaderNetworkServiceObserverTest, DestroyedWithPendingRequest) {
  state_->hold_client_certs_requests = true;
  CreateObserver();

  FakeClientCertificateResponder responder;
  RequestCertificate(responder);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return state_->held_requests.size() == 1; }));

  observer_.reset();

  // The store is destroyed with the observer, and the request is dropped.
  EXPECT_EQ(state_->stores_destroyed.load(), 1);
  EXPECT_EQ(responder.WaitForResponse(), Response::kDisconnected);
}

TEST_F(UrlLoaderNetworkServiceObserverTest,
       ResponderDisconnectDestroysPendingStore) {
  state_->hold_client_certs_requests = true;
  CreateObserver();

  FakeClientCertificateResponder responder;
  RequestCertificate(responder);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return state_->held_requests.size() == 1; }));
  EXPECT_EQ(state_->stores_destroyed.load(), 0);

  responder.Reset();

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return state_->stores_destroyed.load() == 1; }));
}

// The observer can live on a different sequence from the one that makes the
// requests, in which case the stores, identities and private keys are only used
// on the observer's sequence. The fake objects verify this.
TEST_F(UrlLoaderNetworkServiceObserverTest, ObserverOnAnotherSequence) {
  state_->observer_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({});
  base::SequenceBound<UrlLoaderNetworkServiceObserver> observer(
      state_->observer_task_runner, CreateStoreCallback());
  observer.AsyncCall(&UrlLoaderNetworkServiceObserver::BindReceiver)
      .WithArgs(observer_remote_.BindNewPipeAndPassReceiver());

  FakeClientCertificateResponder responder_1;
  RequestCertificate(responder_1);
  ExpectCertificateAndSign(responder_1);
  FakeClientCertificateResponder responder_2;
  RequestCertificate(responder_2);
  ExpectCertificateAndSign(responder_2);

  EXPECT_EQ(state_->create_store_calls.load(), 2);

  // Wait for the stores to be destroyed, which is done by tasks that were
  // posted to the observer's sequence before it responded.
  base::test::TestFuture<void> flushed;
  state_->observer_task_runner->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                                 flushed.GetCallback());
  EXPECT_TRUE(flushed.Wait());
  EXPECT_EQ(state_->stores_destroyed.load(), 2);
}

}  // namespace remoting
