// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/run_until.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "google_apis/gcm/base/fake_encryptor.h"
#include "google_apis/gcm/base/mcs_message.h"
#include "google_apis/gcm/base/mcs_util.h"
#include "google_apis/gcm/engine/connection_factory_impl.h"
#include "google_apis/gcm/engine/fake_mcs_server.h"
#include "google_apis/gcm/engine/gcm_store_impl.h"
#include "google_apis/gcm/engine/mcs_client.h"
#include "google_apis/gcm/monitoring/fake_gcm_stats_recorder.h"
#include "google_apis/gcm/protocol/mcs.pb.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/backoff_entry.h"
#include "net/base/network_change_notifier.h"
#include "net/proxy_resolution/proxy_config_with_annotation.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/proxy_resolving_socket.mojom.h"
#include "services/network/test/fake_test_cert_verifier_params_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

constexpr uint64_t kAndroidId = 54321;
constexpr uint64_t kSecurityToken = 12345;

constexpr net::BackoffEntry::Policy kFastBackoffPolicy = {
    /*num_errors_to_ignore=*/0,
    /*initial_delay_ms=*/1,
    /*multiply_factor=*/2,
    /*jitter_factor=*/0,
    /*maximum_backoff_ms=*/10,
    /*entry_lifetime_ms=*/-1,
    /*always_use_initial_delay=*/false,
};

// End-to-end integration tests exercising `MCSClient`, `ConnectionFactoryImpl`,
// `ConnectionHandlerImpl`, and `GCMStoreImpl` over a real loopback TCP socket
// against `FakeMCSServer`.
class MCSClientIntegrationTest : public testing::Test {
 public:
  MCSClientIntegrationTest()
      : network_connection_tracker_(
            network::TestNetworkConnectionTracker::CreateInstance()),
        task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        network_change_notifier_(
            net::NetworkChangeNotifier::CreateMockIfNeeded()),
        network_service_(network::NetworkService::CreateForTesting()) {
    CHECK(client_store_dir_.CreateUniqueTempDir());
    CHECK(server_store_dir_.CreateUniqueTempDir());
    // `SimpleTestClock` starts at 0, whereas `MCSClient::SendPacketToWire()`
    // requires `DataMessageStanza::sent() > 0` and computes
    // `queued = now - sent`. Advance `clock_` so `now > sent` in tests.
    clock_.Advance(base::Seconds(10));

    network::mojom::NetworkContextParamsPtr params =
        network::mojom::NetworkContextParams::New();
    params->cert_verifier_params =
        network::FakeTestCertVerifierParamsFactory::GetCertVerifierParams();
    params->initial_proxy_config =
        net::ProxyConfigWithAnnotation::CreateDirect();
    network_context_ = std::make_unique<network::NetworkContext>(
        network_service_.get(),
        network_context_remote_.BindNewPipeAndPassReceiver(),
        std::move(params));

    StartNewServer();
  }

  ~MCSClientIntegrationTest() override { DestroyClient(); }

  void StartNewServer() {
    fake_mcs_server_ = std::make_unique<FakeMCSServer>();
    fake_mcs_server_->InitializeAndListen();
    fake_mcs_server_->Start(server_store_dir_.GetPath());
  }

  // Initializes `GCMStoreImpl`, `ConnectionFactoryImpl`, and `MCSClient` and
  // logs in with `kAndroidId` and `kSecurityToken`, waiting for the server's
  // `LoginResponse`.
  void InitializeAndLoginClient() {
    gcm_store_ = std::make_unique<GCMStoreImpl>(
        client_store_dir_.GetPath(),
        task_environment_.GetMainThreadTaskRunner(),
        std::make_unique<FakeEncryptor>());
    connection_factory_ = std::make_unique<ConnectionFactoryImpl>(
        std::vector<GURL>{fake_mcs_server_->GetEndpointURL()},
        kFastBackoffPolicy,
        base::BindRepeating(
            &MCSClientIntegrationTest::CreateProxyResolvingSocketFactory,
            base::Unretained(this)),
        base::SingleThreadTaskRunner::GetCurrentDefault(), &recorder_,
        network_connection_tracker_.get());
    mcs_client_ = std::make_unique<MCSClient>(
        "test-version", &clock_, connection_factory_.get(), gcm_store_.get(),
        base::SingleThreadTaskRunner::GetCurrentDefault(), &recorder_);

    base::test::TestFuture<std::unique_ptr<GCMStore::LoadResult>> load_future;
    gcm_store_->Load(GCMStore::CREATE_IF_MISSING, load_future.GetCallback());
    std::unique_ptr<GCMStore::LoadResult> load_result = load_future.Take();
    load_result->device_android_id = kAndroidId;
    load_result->device_security_token = kSecurityToken;
    mcs_client_->Initialize(
        base::BindRepeating(&MCSClientIntegrationTest::OnMessageReceived,
                            base::Unretained(this)),
        base::BindRepeating(&MCSClientIntegrationTest::OnMessageSent,
                            base::Unretained(this)),
        std::move(load_result));

    base::RunLoop login_loop;
    wait_for_login_closure_ = login_loop.QuitClosure();
    mcs_client_->Login(kAndroidId, kSecurityToken);
    login_loop.Run();
  }

  void DestroyClient() {
    mcs_client_.reset();
    connection_factory_.reset();
    if (gcm_store_) {
      gcm_store_->Close();
      gcm_store_.reset();
      // `GCMStoreImpl::Close()` posts `Backend::Close()` to the blocking task
      // runner (`task_environment_.GetMainThreadTaskRunner()`). Flush it so the
      // underlying LevelDB file lock is released before a subsequent
      // `InitializeAndLoginClient()` reopens `client_store_dir_`.
      base::RunLoop loop;
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, loop.QuitClosure());
      loop.Run();
    }
  }

  mcs_proto::DataMessageStanza WaitForNextDataMessage() {
    if (received_data_messages_.empty()) {
      // Wait until `OnMessageReceived()` receives a `kDataMessageStanzaTag`
      // packet, appends it to `received_data_messages_`, and invokes
      // `wait_for_data_message_closure_`.
      base::RunLoop loop;
      wait_for_data_message_closure_ = loop.QuitClosure();
      loop.Run();
    }
    CHECK(!received_data_messages_.empty())
        << "Timed out waiting for OnMessageReceived() to receive a "
           "DataMessageStanza";
    mcs_proto::DataMessageStanza stanza =
        std::move(received_data_messages_.front());
    received_data_messages_.erase(received_data_messages_.begin());
    return stanza;
  }

  void DisconnectAllClientsAndWaitForRelogin() {
    base::RunLoop relogin_loop;
    wait_for_login_closure_ = relogin_loop.QuitClosure();
    fake_mcs_server_->DisconnectAllClients();
    relogin_loop.Run();
  }

  void SendClientMessageAndWaitForSent(const MCSMessage& message) {
    base::RunLoop sent_loop;
    wait_for_message_sent_closure_ = sent_loop.QuitClosure();
    mcs_client_->SendMessage(message);
    sent_loop.Run();
  }

  FakeMCSServer& fake_mcs_server() { return *fake_mcs_server_; }

  const std::vector<mcs_proto::DataMessageStanza>& received_data_messages()
      const {
    return received_data_messages_;
  }

  const std::vector<std::pair<std::string, MCSClient::MessageSendStatus>>&
  sent_statuses() const {
    return sent_statuses_;
  }

 private:
  void CreateProxyResolvingSocketFactory(
      mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>
          receiver) {
    network_context_->CreateProxyResolvingSocketFactory(std::move(receiver));
  }

  void OnMessageReceived(const MCSMessage& message) {
    if (message.tag() == kLoginResponseTag) {
      if (wait_for_login_closure_) {
        std::move(wait_for_login_closure_).Run();
      }
      return;
    }
    if (message.tag() == kDataMessageStanzaTag) {
      const auto& stanza = static_cast<const mcs_proto::DataMessageStanza&>(
          message.GetProtobuf());
      received_data_messages_.push_back(stanza);
      if (wait_for_data_message_closure_) {
        std::move(wait_for_data_message_closure_).Run();
      }
    }
  }

  void OnMessageSent(const std::string& app_id,
                     const std::string& message_id,
                     MCSClient::MessageSendStatus status) {
    sent_statuses_.emplace_back(message_id, status);
    if (status == MCSClient::SENT && wait_for_message_sent_closure_) {
      std::move(wait_for_message_sent_closure_).Run();
    }
  }

  std::unique_ptr<network::TestNetworkConnectionTracker>
      network_connection_tracker_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  std::unique_ptr<network::NetworkService> network_service_;
  mojo::Remote<network::mojom::NetworkContext> network_context_remote_;
  std::unique_ptr<network::NetworkContext> network_context_;

  base::ScopedTempDir client_store_dir_;
  base::ScopedTempDir server_store_dir_;
  base::SimpleTestClock clock_;
  FakeGCMStatsRecorder recorder_;

  std::unique_ptr<FakeMCSServer> fake_mcs_server_;
  std::unique_ptr<GCMStore> gcm_store_;
  std::unique_ptr<ConnectionFactoryImpl> connection_factory_;
  std::unique_ptr<MCSClient> mcs_client_;

  base::OnceClosure wait_for_login_closure_;
  base::OnceClosure wait_for_data_message_closure_;
  base::OnceClosure wait_for_message_sent_closure_;
  std::vector<mcs_proto::DataMessageStanza> received_data_messages_;
  std::vector<std::pair<std::string, MCSClient::MessageSendStatus>>
      sent_statuses_;
};

TEST_F(MCSClientIntegrationTest, LoginAndReceiveServerToClientDataMessage) {
  InitializeAndLoginClient();

  mcs_proto::DataMessageStanza stanza;
  stanza.set_from("sender-id");
  stanza.set_category("com.chrome.test");
  stanza.set_immediate_ack(true);
  mcs_proto::AppData* kv = stanza.add_app_data();
  kv->set_key("payload");
  kv->set_value("hello-from-server");

  fake_mcs_server().SendDataMessage(kAndroidId, stanza);

  mcs_proto::DataMessageStanza received = WaitForNextDataMessage();
  EXPECT_EQ(received.from(), "sender-id");
  EXPECT_EQ(received.category(), "com.chrome.test");
  ASSERT_EQ(received.app_data_size(), 1);
  EXPECT_EQ(received.app_data(0).key(), "payload");
  EXPECT_EQ(received.app_data(0).value(), "hello-from-server");
  EXPECT_FALSE(received.persistent_id().empty());
}

TEST_F(MCSClientIntegrationTest,
       DeliverMessagesQueuedBeforeLoginAndDoNotRedeliverAfterAck) {
  mcs_proto::DataMessageStanza msg1;
  msg1.set_from("sender-1");
  msg1.set_category("com.chrome.test");
  msg1.set_immediate_ack(true);
  fake_mcs_server().SendDataMessage(kAndroidId, msg1);

  InitializeAndLoginClient();
  mcs_proto::DataMessageStanza received1 = WaitForNextDataMessage();
  EXPECT_EQ(received1.from(), "sender-1");

  // Wait until the client's immediate StreamAck has been processed by the
  // server before restarting.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !fake_mcs_server().HasUnacknowledgedMessages(kAndroidId);
  }));

  // Restart the client and server (using the same persistent directories) and
  // verify that `msg1` is not redelivered while a new `msg2` is delivered.
  DestroyClient();
  StartNewServer();

  mcs_proto::DataMessageStanza msg2;
  msg2.set_from("sender-2");
  msg2.set_category("com.chrome.test");
  msg2.set_immediate_ack(true);
  fake_mcs_server().SendDataMessage(kAndroidId, msg2);

  InitializeAndLoginClient();
  mcs_proto::DataMessageStanza received2 = WaitForNextDataMessage();
  EXPECT_EQ(received2.from(), "sender-2");
  EXPECT_TRUE(received_data_messages().empty());
}

TEST_F(MCSClientIntegrationTest, AcknowledgeUnackedMessageInNextLoginRequest) {
  InitializeAndLoginClient();

  // Send `msg1` without `immediate_ack`, so `MCSClient` records the message's
  // `persistent_id` in `GCMStore` and acknowledges it in `LoginRequest` upon
  // the next connection.
  mcs_proto::DataMessageStanza msg1;
  msg1.set_from("sender-unacked");
  msg1.set_category("com.chrome.test");
  fake_mcs_server().SendDataMessage(kAndroidId, msg1);

  mcs_proto::DataMessageStanza received1 = WaitForNextDataMessage();
  EXPECT_EQ(received1.from(), "sender-unacked");

  // Restart the client (preserving `GCMStore` on disk) and reconnect. The new
  // `LoginRequest` will carry `received1.persistent_id()` in
  // `received_persistent_id`, so `FakeMCSServer` should drop `msg1` and only
  // deliver `msg2`.
  DestroyClient();

  mcs_proto::DataMessageStanza msg2;
  msg2.set_from("sender-after-reconnect");
  msg2.set_category("com.chrome.test");
  fake_mcs_server().SendDataMessage(kAndroidId, msg2);

  InitializeAndLoginClient();
  mcs_proto::DataMessageStanza received2 = WaitForNextDataMessage();
  EXPECT_EQ(received2.from(), "sender-after-reconnect");
  EXPECT_TRUE(received_data_messages().empty());
}

TEST_F(MCSClientIntegrationTest,
       ReconnectAfterServerDisconnectAndReceiveQueuedMessage) {
  InitializeAndLoginClient();

  mcs_proto::DataMessageStanza msg1;
  msg1.set_from("sender-before-disconnect");
  msg1.set_category("com.chrome.test");
  fake_mcs_server().SendDataMessage(kAndroidId, msg1);
  EXPECT_EQ(WaitForNextDataMessage().from(), "sender-before-disconnect");

  // Simulate a server-side TCP disconnect; `ConnectionFactoryImpl` will detect
  // the closed socket, reconnect with `kFastBackoffPolicy`, and `MCSClient`
  // will send a new `LoginRequest` acknowledging `msg1`.
  DisconnectAllClientsAndWaitForRelogin();

  mcs_proto::DataMessageStanza msg2;
  msg2.set_from("sender-after-disconnect");
  msg2.set_category("com.chrome.test");
  fake_mcs_server().SendDataMessage(kAndroidId, msg2);
  EXPECT_EQ(WaitForNextDataMessage().from(), "sender-after-disconnect");
  EXPECT_TRUE(received_data_messages().empty());
}

TEST_F(MCSClientIntegrationTest, SendClientToServerDataMessage) {
  InitializeAndLoginClient();

  base::test::TestFuture<uint64_t, mcs_proto::DataMessageStanza> server_future;
  fake_mcs_server().SetDataMessageReceivedCallback(
      server_future.GetRepeatingCallback<
          uint64_t, const mcs_proto::DataMessageStanza&>());

  mcs_proto::DataMessageStanza upstream;
  upstream.set_id("upstream-msg-1");
  upstream.set_from("client-app");
  upstream.set_to("server-recipient");
  upstream.set_category("com.chrome.test");
  upstream.set_ttl(300);
  upstream.set_sent(1);
  upstream.set_device_user_id(1);
  mcs_proto::AppData* kv = upstream.add_app_data();
  kv->set_key("body");
  kv->set_value("hello-from-client");

  // `FakeMCSServer` replies with a `StreamAck`, causing `MCSClient` to report
  // `MCSClient::SENT`.
  SendClientMessageAndWaitForSent(MCSMessage(kDataMessageStanzaTag, upstream));

  auto [received_android_id, received_stanza] = server_future.Take();
  EXPECT_EQ(received_android_id, kAndroidId);
  EXPECT_EQ(received_stanza.id(), "upstream-msg-1");
  EXPECT_EQ(received_stanza.from(), "client-app");
  EXPECT_EQ(received_stanza.to(), "server-recipient");
  EXPECT_EQ(received_stanza.category(), "com.chrome.test");
  ASSERT_EQ(received_stanza.app_data_size(), 1);
  EXPECT_EQ(received_stanza.app_data(0).key(), "body");
  EXPECT_EQ(received_stanza.app_data(0).value(), "hello-from-client");

  ASSERT_EQ(sent_statuses().size(), 2u);
  EXPECT_EQ(sent_statuses()[0],
            std::make_pair(std::string("upstream-msg-1"), MCSClient::QUEUED));
  EXPECT_EQ(sent_statuses()[1],
            std::make_pair(std::string("upstream-msg-1"), MCSClient::SENT));
}

}  // namespace

}  // namespace gcm
