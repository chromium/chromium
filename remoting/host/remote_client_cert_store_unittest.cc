// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/remote_client_cert_store.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/thread.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_listener.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
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
    ++get_certificates_call_count_;
    if (hold_next_call_) {
      hold_next_call_ = false;
      held_callback_ = std::move(callback);
      if (on_get_certificates_called_) {
        std::move(on_get_certificates_called_).Run();
      }
      return;
    }
    RunCallbackWithCerts(std::move(callback));
  }

  void HoldNextGetCertificatesCall(base::OnceClosure on_called) {
    hold_next_call_ = true;
    on_get_certificates_called_ = std::move(on_called);
  }

  void ResumeHeldGetCertificatesCall() {
    ASSERT_TRUE(held_callback_);
    RunCallbackWithCerts(std::move(held_callback_));
  }

  void AddCertificate(scoped_refptr<net::X509Certificate> cert,
                      const std::string& provider_name,
                      const std::vector<uint16_t>& algorithm_preferences) {
    cert_infos_.push_back(
        {std::move(cert), provider_name, algorithm_preferences});
  }

  bool get_certificates_called() const {
    return get_certificates_call_count_ > 0;
  }
  int get_certificates_call_count() const {
    return get_certificates_call_count_;
  }

 private:
  struct CertInfo {
    scoped_refptr<net::X509Certificate> cert;
    std::string provider_name;
    std::vector<uint16_t> algorithm_preferences;
  };

  void RunCallbackWithCerts(GetCertificatesCallback callback) {
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

  mojo::AssociatedReceiver<mojom::CertificateBroker> receiver_{this};
  std::vector<CertInfo> cert_infos_;
  int get_certificates_call_count_ = 0;
  bool hold_next_call_ = false;
  base::OnceClosure on_get_certificates_called_;
  GetCertificatesCallback held_callback_;
};

class ChannelConnectListener : public IPC::Listener {
 public:
  explicit ChannelConnectListener(base::OnceClosure on_connected,
                                  mojom::CertificateBroker* broker = nullptr)
      : on_connected_(std::move(on_connected)), broker_(broker) {}
  ~ChannelConnectListener() override = default;

  void OnChannelConnected(int32_t peer_pid) override {
    if (on_connected_) {
      std::move(on_connected_).Run();
    }
  }

  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override {
    if (broker_ && interface_name == mojom::CertificateBroker::Name_) {
      receivers_.Add(broker_.get(),
                     mojo::PendingAssociatedReceiver<mojom::CertificateBroker>(
                         std::move(handle)));
    }
  }

 private:
  base::OnceClosure on_connected_;
  raw_ptr<mojom::CertificateBroker> broker_;
  mojo::AssociatedReceiverSet<mojom::CertificateBroker> receivers_;
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

// TODO(crbug.com/582897560): Remove along with the PostTask workaround in
// RemoteClientCertStore::GetClientCerts() once https://crrev.com/c/8856523
// lands.
TEST_F(RemoteClientCertStoreTest, DeleteStoreBeforeDeferredCallCancelsRequest) {
  broker_.AddCertificate(test_cert_, "TestProvider", {kTestAlgorithm});

  auto store =
      std::make_unique<RemoteClientCertStore>(broker_.BindNewEndpoint());

  base::test::TestFuture<net::ClientCertIdentityList> future;
  store->GetClientCerts(base::MakeRefCounted<net::SSLCertRequestInfo>(),
                        future.GetCallback());
  store.reset();

  base::test::TestFuture<void> flushed;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, flushed.GetCallback());
  ASSERT_TRUE(flushed.Wait());

  EXPECT_FALSE(broker_.get_certificates_called());
  EXPECT_FALSE(future.IsReady());
}

TEST_F(RemoteClientCertStoreTest, DeleteStoreWhileRequestInFlightDoesNotCrash) {
  broker_.AddCertificate(test_cert_, "TestProvider", {kTestAlgorithm});
  base::test::TestFuture<void> broker_called;
  broker_.HoldNextGetCertificatesCall(broker_called.GetCallback());

  auto store =
      std::make_unique<RemoteClientCertStore>(broker_.BindNewEndpoint());

  base::test::TestFuture<net::ClientCertIdentityList> future;
  store->GetClientCerts(base::MakeRefCounted<net::SSLCertRequestInfo>(),
                        future.GetCallback());
  ASSERT_TRUE(broker_called.Wait());

  // Deleting the store while the Mojo response is in flight destroys the
  // AssociatedRemote and cancels the pending response callback cleanly.
  store.reset();
  broker_.ResumeHeldGetCertificatesCall();

  base::test::TestFuture<void> flushed;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, flushed.GetCallback());
  ASSERT_TRUE(flushed.Wait());

  EXPECT_FALSE(future.IsReady());
}

TEST_F(RemoteClientCertStoreTest,
       ImmediateGetClientCertsOnChannelProxyIpcThread) {
  broker_.AddCertificate(test_cert_, "TestProvider", {kTestAlgorithm});

  base::Thread ipc_thread("ipc_thread");
  ASSERT_TRUE(ipc_thread.StartWithOptions(
      base::Thread::Options(base::MessagePumpType::IO, 0)));

  base::test::TestFuture<void> server_connected;
  base::test::TestFuture<void> client_connected;
  ChannelConnectListener server_listener(server_connected.GetCallback(),
                                         &broker_);
  ChannelConnectListener client_listener(client_connected.GetCallback());

  mojo::MessagePipe pipe;
  auto server_channel = IPC::ChannelProxy::Create(
      std::move(pipe.handle0), IPC::Channel::MODE_SERVER, &server_listener,
      ipc_thread.task_runner(),
      base::SingleThreadTaskRunner::GetCurrentDefault());
  auto client_channel = IPC::ChannelProxy::Create(
      std::move(pipe.handle1), IPC::Channel::MODE_CLIENT, &client_listener,
      ipc_thread.task_runner(),
      base::SingleThreadTaskRunner::GetCurrentDefault());

  ASSERT_TRUE(server_connected.Wait());
  ASSERT_TRUE(client_connected.Wait());

  using ScopedRemoteStore =
      std::unique_ptr<RemoteClientCertStore, base::OnTaskRunnerDeleter>;

  // Mirror HostProcess::CreateRemoteClientCertStore() +
  // UrlLoaderNetworkServiceObserver::OnCertificateRequested(): request a fresh
  // CertificateBroker associated interface and immediately call
  // GetClientCerts() synchronously on the ChannelProxy's IPC thread.
  for (int attempt = 0; attempt < 2; ++attempt) {
    ScopedRemoteStore store(
        nullptr, base::OnTaskRunnerDeleter(ipc_thread.task_runner()));
    base::test::TestFuture<net::ClientCertIdentityList> certs_future;
    auto certs_callback =
        base::BindPostTaskToCurrentDefault(certs_future.GetCallback());

    ipc_thread.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](IPC::ChannelProxy* channel, ScopedRemoteStore* store_out,
               net::ClientCertStore::ClientCertListCallback callback) {
              mojo::PendingAssociatedRemote<mojom::CertificateBroker>
                  broker_remote;
              channel->GetRemoteAssociatedInterface(
                  broker_remote.InitWithNewEndpointAndPassReceiver());
              store_out->reset(
                  new RemoteClientCertStore(std::move(broker_remote)));
              (*store_out)
                  ->GetClientCerts(
                      base::MakeRefCounted<net::SSLCertRequestInfo>(),
                      std::move(callback));
            },
            base::Unretained(client_channel.get()), base::Unretained(&store),
            std::move(certs_callback)));

    ASSERT_TRUE(certs_future.Wait());
    net::ClientCertIdentityList result_certs = certs_future.Take();
    EXPECT_EQ(broker_.get_certificates_call_count(), attempt + 1);
    ASSERT_EQ(result_certs.size(), 1u);
    EXPECT_TRUE(
        result_certs[0]->certificate()->EqualsExcludingChain(test_cert_.get()));

    store.reset();
    base::test::TestFuture<void> destroy_store_future;
    ipc_thread.task_runner()->PostTaskAndReply(
        FROM_HERE, base::DoNothing(), destroy_store_future.GetCallback());
    ASSERT_TRUE(destroy_store_future.Wait());
  }

  client_channel.reset();
  server_channel.reset();
  ipc_thread.Stop();
}

}  // namespace remoting
