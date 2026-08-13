// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ipc_peer_session.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "remoting/base/session_options.h"
#include "remoting/base/session_policies.h"
#include "remoting/base/source_location.h"
#include "remoting/host/base/desktop_environment_options.h"
#include "remoting/host/mojom/peer_session.mojom.h"
#include "remoting/host/mojom/webrtc_types.mojom.h"
#include "remoting/protocol/errors.h"
#include "remoting/protocol/ice_config.h"
#include "remoting/protocol/ice_config_fetcher.h"
#include "remoting/protocol/transport.h"
#include "remoting/signaling/jingle_data_structures.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

class FakeIceConfigFetcher : public protocol::IceConfigFetcher {
 public:
  explicit FakeIceConfigFetcher(protocol::IceConfig config)
      : config_(std::move(config)) {}
  ~FakeIceConfigFetcher() override = default;

  void GetIceConfig(OnIceConfigCallback callback) override {
    std::move(callback).Run(config_);
  }

 private:
  protocol::IceConfig config_;
};

class MockPeerSessionReceiver : public mojom::PeerSession {
 public:
  MockPeerSessionReceiver() = default;
  ~MockPeerSessionReceiver() override = default;

  void Bind(mojo::PendingReceiver<mojom::PeerSession> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  void set_disconnect_handler(base::OnceClosure handler) {
    receiver_.set_disconnect_handler(std::move(handler));
  }

  void set_start_closure(base::OnceClosure closure) {
    start_closure_ = std::move(closure);
  }

  void set_transport_closure(base::OnceClosure closure) {
    transport_closure_ = std::move(closure);
  }

  bool is_bound() const { return receiver_.is_bound(); }
  bool start_called() const { return start_called_; }
  bool start_transport_called() const { return start_transport_called_; }
  bool process_transport_info_called() const {
    return process_transport_info_called_;
  }
  const std::string& last_auth_key() const { return last_auth_key_; }
  const std::string& last_client_jid() const { return last_client_jid_; }
  const JingleTransportInfo& last_transport_info() const {
    return last_transport_info_;
  }
  const SessionPolicies& last_session_policies() const {
    return last_session_policies_;
  }
  const SessionOptions& last_session_options() const {
    return last_session_options_;
  }

  // mojom::PeerSession implementation:
  void Start(const std::string& client_jid,
             mojo::PendingRemote<mojom::PeerSessionEventHandler> event_handler,
             mojo::PendingRemote<mojom::DesktopSession> desktop_control,
             mojo::PendingReceiver<mojom::DesktopSessionEvents>
                 desktop_events_receiver,
             mojo::PendingRemote<mojom::IceConfigFetcher> ice_config_fetcher,
             mojo::PendingRemote<mojom::PairingRequester> pairing_requester,
             const DesktopEnvironmentOptions& desktop_environment_options,
             const SessionPolicies& session_policies,
             const SessionOptions& session_options) override {
    start_called_ = true;
    if (event_handler) {
      event_handler_remote_.Bind(std::move(event_handler));
    }
    if (ice_config_fetcher) {
      ice_config_fetcher_remote_.Bind(std::move(ice_config_fetcher));
    }
    if (pairing_requester) {
      pairing_requester_remote_.Bind(std::move(pairing_requester));
    }
    last_client_jid_ = client_jid;
    last_session_policies_ = session_policies;
    last_session_options_ = session_options;
    if (start_closure_) {
      std::move(start_closure_).Run();
    }
  }

  void StartTransport(const std::string& auth_key,
                      mojo::PendingRemote<mojom::TransportEventHandler>
                          transport_event_handler) override {
    start_transport_called_ = true;
    last_auth_key_ = auth_key;
    if (transport_event_handler) {
      transport_event_handler_.Bind(std::move(transport_event_handler));
    }
    if (transport_closure_) {
      std::move(transport_closure_).Run();
    }
  }

  void ProcessTransportInfo(
      const JingleTransportInfo& transport_info) override {
    process_transport_info_called_ = true;
    last_transport_info_ = transport_info;
    if (transport_closure_) {
      std::move(transport_closure_).Run();
    }
  }

  void DisconnectSession(protocol::ErrorCode error,
                         const std::string& error_details,
                         const SourceLocation& error_location) override {}

  mojo::Remote<mojom::TransportEventHandler>& transport_event_handler() {
    return transport_event_handler_;
  }
  mojo::Remote<mojom::IceConfigFetcher>& ice_config_fetcher_remote() {
    return ice_config_fetcher_remote_;
  }
  mojo::Remote<mojom::PairingRequester>& pairing_requester_remote() {
    return pairing_requester_remote_;
  }

 private:
  mojo::Receiver<mojom::PeerSession> receiver_{this};
  mojo::Remote<mojom::PeerSessionEventHandler> event_handler_remote_;
  mojo::Remote<mojom::TransportEventHandler> transport_event_handler_;
  mojo::Remote<mojom::IceConfigFetcher> ice_config_fetcher_remote_;
  mojo::Remote<mojom::PairingRequester> pairing_requester_remote_;
  base::OnceClosure start_closure_;
  base::OnceClosure transport_closure_;
  bool start_called_ = false;
  bool start_transport_called_ = false;
  bool process_transport_info_called_ = false;
  std::string last_auth_key_;
  std::string last_client_jid_;
  JingleTransportInfo last_transport_info_;
  SessionPolicies last_session_policies_;
  SessionOptions last_session_options_;
};

class MockPeerSessionManager : public mojom::PeerSessionManager {
 public:
  MockPeerSessionManager() = default;
  ~MockPeerSessionManager() override = default;

  void Bind(
      mojo::PendingAssociatedReceiver<mojom::PeerSessionManager> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  void set_quit_closure(base::OnceClosure closure) {
    quit_closure_ = std::move(closure);
  }

  bool launch_called() const { return launch_called_; }

  // mojom::PeerSessionManager implementation:
  void LaunchPeerSession(
      mojo::PendingReceiver<mojom::PeerSession> receiver) override {
    launch_called_ = true;
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

 private:
  mojo::AssociatedReceiver<mojom::PeerSessionManager> receiver_{this};
  bool launch_called_ = false;
  base::OnceClosure quit_closure_;
};

class MockDesktopSessionManager : public mojom::DesktopSessionManager {
 public:
  MockDesktopSessionManager() = default;
  ~MockDesktopSessionManager() override = default;

  void Bind(
      mojo::PendingAssociatedReceiver<mojom::DesktopSessionManager> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  void set_quit_closure(base::OnceClosure closure) {
    quit_closure_ = std::move(closure);
  }

  // mojom::DesktopSessionManager implementation:
  void GetDesktopSession(
      mojo::PendingReceiver<mojom::DesktopSession> control_receiver,
      mojo::PendingRemote<mojom::DesktopSessionEvents> events_remote,
      mojom::DesktopSessionOptionsPtr options) override {
    get_desktop_session_called_ = true;
    last_options_ = std::move(options);
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  bool get_desktop_session_called() const {
    return get_desktop_session_called_;
  }

  const mojom::DesktopSessionOptionsPtr& last_options() const {
    return last_options_;
  }

 private:
  mojo::AssociatedReceiver<mojom::DesktopSessionManager> receiver_{this};
  bool get_desktop_session_called_ = false;
  mojom::DesktopSessionOptionsPtr last_options_;
  base::OnceClosure quit_closure_;
};

class MockEventHandler : public PeerSession::EventHandler {
 public:
  MockEventHandler() = default;
  ~MockEventHandler() override = default;

  void OnSessionChannelsConnected() override {}
  void OnSessionClosed(protocol::ErrorCode error,
                       const std::string& error_details,
                       const SourceLocation& error_location) override {}
  void OnSessionRouteChange(const std::string& channel_name,
                            const protocol::TransportRoute& route) override {}
};

}  // namespace

class IpcPeerSessionTest : public testing::Test {
 public:
  IpcPeerSessionTest() = default;
  ~IpcPeerSessionTest() override = default;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(IpcPeerSessionTest, DisconnectHandlerFiresWhenSessionDestroyed) {
  mojo::PendingRemote<mojom::PeerSession> remote;
  MockPeerSessionReceiver receiver;
  receiver.Bind(remote.InitWithNewPipeAndPassReceiver());

  auto session = std::make_unique<IpcPeerSession>(
      std::move(remote), base::DoNothing(),
      std::make_unique<FakeIceConfigFetcher>(protocol::IceConfig()),
      base::NullCallback());
  EXPECT_TRUE(receiver.is_bound());

  base::RunLoop run_loop;
  bool disconnected = false;
  receiver.set_disconnect_handler(base::BindLambdaForTesting([&]() {
    disconnected = true;
    run_loop.Quit();
  }));

  session.reset();
  run_loop.Run();

  EXPECT_TRUE(disconnected);
}

TEST_F(IpcPeerSessionTest, CallingStubbedMethodsDoesNotCrash) {
  mojo::PendingRemote<mojom::PeerSession> remote;
  MockPeerSessionReceiver receiver;
  receiver.Bind(remote.InitWithNewPipeAndPassReceiver());

  auto session = std::make_unique<IpcPeerSession>(
      std::move(remote), base::DoNothing(),
      std::make_unique<FakeIceConfigFetcher>(protocol::IceConfig()),
      base::NullCallback());
  MockEventHandler event_handler;
  session->Start(&event_handler, "", DesktopEnvironmentOptions(), {},
                 SessionPolicies(), SessionOptions());
  session->DisconnectSession(protocol::ErrorCode::OK, "", FROM_HERE);
  EXPECT_EQ(session->transport(), session.get());
}

TEST_F(IpcPeerSessionTest, TransportStartPropagatesAuthKey) {
  mojo::PendingRemote<mojom::PeerSession> remote;
  MockPeerSessionReceiver receiver;
  receiver.Bind(remote.InitWithNewPipeAndPassReceiver());

  auto session = std::make_unique<IpcPeerSession>(
      std::move(remote), base::DoNothing(),
      std::make_unique<FakeIceConfigFetcher>(protocol::IceConfig()),
      base::NullCallback());
  MockEventHandler event_handler;
  session->Start(&event_handler, "", DesktopEnvironmentOptions(), {},
                 SessionPolicies(), SessionOptions());

  base::RunLoop run_loop;
  receiver.set_transport_closure(run_loop.QuitClosure());

  session->transport()->Start("secret_auth_key", base::DoNothing());
  run_loop.Run();

  EXPECT_TRUE(receiver.start_transport_called());
  EXPECT_EQ(receiver.last_auth_key(), "secret_auth_key");
  EXPECT_TRUE(receiver.transport_event_handler().is_bound());
}

TEST_F(IpcPeerSessionTest, TransportProcessTransportInfoCallsRemote) {
  mojo::PendingRemote<mojom::PeerSession> remote;
  MockPeerSessionReceiver receiver;
  receiver.Bind(remote.InitWithNewPipeAndPassReceiver());

  auto session = std::make_unique<IpcPeerSession>(
      std::move(remote), base::DoNothing(),
      std::make_unique<FakeIceConfigFetcher>(protocol::IceConfig()),
      base::NullCallback());
  MockEventHandler event_handler;
  session->Start(&event_handler, "", DesktopEnvironmentOptions(), {},
                 SessionPolicies(), SessionOptions());

  base::RunLoop run_loop;
  receiver.set_transport_closure(run_loop.QuitClosure());

  JingleTransportInfo transport_info;
  webrtc::Candidate candidate(
      1, "udp", webrtc::SocketAddress("192.168.1.10", 5000), 2130706431, "user",
      "pass", webrtc::IceCandidateType::kHost, 0, "found1", 0, 0);
  transport_info.candidates.push_back(
      IceTransportInfo::NamedCandidate("audio", candidate, 0));
  session->transport()->ProcessTransportInfo(transport_info);
  run_loop.Run();

  EXPECT_TRUE(receiver.process_transport_info_called());
  ASSERT_EQ(receiver.last_transport_info().candidates.size(), 1u);
  EXPECT_EQ(receiver.last_transport_info().candidates[0].name, "audio");
}

TEST_F(IpcPeerSessionTest, SendTransportInfoDispatchesToCallback) {
  mojo::PendingRemote<mojom::PeerSession> remote;
  MockPeerSessionReceiver receiver;
  receiver.Bind(remote.InitWithNewPipeAndPassReceiver());

  auto session = std::make_unique<IpcPeerSession>(
      std::move(remote), base::DoNothing(),
      std::make_unique<FakeIceConfigFetcher>(protocol::IceConfig()),
      base::NullCallback());
  MockEventHandler event_handler;

  base::RunLoop start_loop;
  receiver.set_start_closure(start_loop.QuitClosure());
  session->Start(&event_handler, "", DesktopEnvironmentOptions(), {},
                 SessionPolicies(), SessionOptions());
  start_loop.Run();

  ASSERT_TRUE(receiver.start_called());

  base::RunLoop run_loop;
  std::unique_ptr<JingleTransportInfo> received_info;

  base::RunLoop start_transport_loop;
  receiver.set_transport_closure(start_transport_loop.QuitClosure());

  session->transport()->Start(
      "auth_key", base::BindLambdaForTesting(
                      [&](std::unique_ptr<JingleTransportInfo> info) {
                        received_info = std::move(info);
                        run_loop.Quit();
                      }));
  start_transport_loop.Run();

  ASSERT_TRUE(receiver.transport_event_handler().is_bound());

  JingleTransportInfo transport_info;
  webrtc::Candidate candidate(
      1, "udp", webrtc::SocketAddress("192.168.1.10", 5000), 2130706431, "user",
      "pass", webrtc::IceCandidateType::kHost, 0, "found1", 0, 0);
  transport_info.candidates.push_back(
      IceTransportInfo::NamedCandidate("video", candidate, 1));
  receiver.transport_event_handler()->SendTransportInfo(
      std::move(transport_info));
  run_loop.Run();

  ASSERT_NE(received_info, nullptr);
  ASSERT_EQ(received_info->candidates.size(), 1u);
  EXPECT_EQ(received_info->candidates[0].name, "video");
}

TEST_F(IpcPeerSessionTest, UnboundTransportOperationsDoNotCrash) {
  auto session = std::make_unique<IpcPeerSession>(
      mojo::NullRemote(), base::DoNothing(),
      std::make_unique<FakeIceConfigFetcher>(protocol::IceConfig()),
      base::NullCallback());
  EXPECT_EQ(session->transport(), session.get());

  session->transport()->Start("auth_key", base::DoNothing());

  JingleTransportInfo transport_info;
  EXPECT_TRUE(session->transport()->ProcessTransportInfo(transport_info));
}

TEST_F(IpcPeerSessionTest, CreateInvokesLaunchPeerSession) {
  mojo::AssociatedRemote<mojom::PeerSessionManager> peer_remote;
  mojo::PendingAssociatedReceiver<mojom::PeerSessionManager>
      peer_pending_receiver = peer_remote.BindNewEndpointAndPassReceiver();
  peer_pending_receiver.EnableUnassociatedUsage();

  mojo::AssociatedRemote<mojom::DesktopSessionManager> desktop_remote;
  mojo::PendingAssociatedReceiver<mojom::DesktopSessionManager>
      desktop_pending_receiver =
          desktop_remote.BindNewEndpointAndPassReceiver();
  desktop_pending_receiver.EnableUnassociatedUsage();

  MockPeerSessionManager mock_peer_manager;
  mock_peer_manager.Bind(std::move(peer_pending_receiver));

  MockDesktopSessionManager mock_desktop_manager;
  mock_desktop_manager.Bind(std::move(desktop_pending_receiver));

  IpcPeerSessionFactory factory(
      peer_remote.Unbind(), desktop_remote.Unbind(),
      base::BindRepeating([]() -> std::unique_ptr<protocol::IceConfigFetcher> {
        return std::make_unique<FakeIceConfigFetcher>(protocol::IceConfig());
      }));

  base::RunLoop run_loop;
  mock_desktop_manager.set_quit_closure(run_loop.QuitClosure());

  std::unique_ptr<PeerSession> session = factory.Create();
  EXPECT_NE(session, nullptr);

  MockEventHandler event_handler;
  session->Start(&event_handler, "", DesktopEnvironmentOptions(), {},
                 SessionPolicies(), SessionOptions());

  run_loop.Run();
  EXPECT_TRUE(mock_peer_manager.launch_called());
  EXPECT_TRUE(mock_desktop_manager.get_desktop_session_called());
}

TEST_F(IpcPeerSessionTest,
       SetRequiredUsernamePropagatesToDesktopSessionOptions) {
  mojo::AssociatedRemote<mojom::PeerSessionManager> peer_remote;
  mojo::PendingAssociatedReceiver<mojom::PeerSessionManager>
      peer_pending_receiver = peer_remote.BindNewEndpointAndPassReceiver();
  peer_pending_receiver.EnableUnassociatedUsage();

  mojo::AssociatedRemote<mojom::DesktopSessionManager> desktop_remote;
  mojo::PendingAssociatedReceiver<mojom::DesktopSessionManager>
      desktop_pending_receiver =
          desktop_remote.BindNewEndpointAndPassReceiver();
  desktop_pending_receiver.EnableUnassociatedUsage();

  MockPeerSessionManager mock_peer_manager;
  mock_peer_manager.Bind(std::move(peer_pending_receiver));

  MockDesktopSessionManager mock_desktop_manager;
  mock_desktop_manager.Bind(std::move(desktop_pending_receiver));

  IpcPeerSessionFactory factory(
      peer_remote.Unbind(), desktop_remote.Unbind(),
      base::BindRepeating([]() -> std::unique_ptr<protocol::IceConfigFetcher> {
        return std::make_unique<FakeIceConfigFetcher>(protocol::IceConfig());
      }));
  factory.SetRequiredUsername("testuser");

  base::RunLoop run_loop;
  mock_desktop_manager.set_quit_closure(run_loop.QuitClosure());

  std::unique_ptr<PeerSession> session = factory.Create();
  EXPECT_NE(session, nullptr);

  MockEventHandler event_handler;
  session->Start(&event_handler, "", DesktopEnvironmentOptions(), {},
                 SessionPolicies(), SessionOptions());

  run_loop.Run();
  EXPECT_TRUE(mock_desktop_manager.get_desktop_session_called());
  ASSERT_TRUE(mock_desktop_manager.last_options());
  EXPECT_EQ(mock_desktop_manager.last_options()->required_username, "testuser");
}

TEST_F(IpcPeerSessionTest, StartPropagatesSessionPoliciesAndOptions) {
  mojo::PendingRemote<mojom::PeerSession> remote;
  MockPeerSessionReceiver receiver;
  receiver.Bind(remote.InitWithNewPipeAndPassReceiver());

  base::RunLoop run_loop;
  receiver.set_start_closure(run_loop.QuitClosure());

  IpcPeerSession session(
      std::move(remote),
      base::BindLambdaForTesting(
          [](mojo::PendingReceiver<mojom::DesktopSession> control_receiver,
             mojo::PendingRemote<mojom::DesktopSessionEvents> events_remote,
             mojom::DesktopSessionOptionsPtr options) {}),
      std::make_unique<FakeIceConfigFetcher>(protocol::IceConfig()),
      base::NullCallback());

  SessionPolicies policies;
  policies.allow_file_transfer = false;
  policies.clipboard_size_bytes = 2048;

  SessionOptions options;
  options.detect_updated_region = true;
  options.disable_udp = true;

  MockEventHandler event_handler;
  session.Start(&event_handler, "test_client_jid", DesktopEnvironmentOptions(),
                {}, policies, options);

  run_loop.Run();
  EXPECT_TRUE(receiver.start_called());
  EXPECT_EQ(receiver.last_session_policies().allow_file_transfer, false);
  EXPECT_EQ(receiver.last_session_policies().clipboard_size_bytes, 2048u);
  EXPECT_EQ(receiver.last_session_options().detect_updated_region, true);
  EXPECT_EQ(receiver.last_session_options().disable_udp, true);
}

TEST_F(IpcPeerSessionTest, CreateReturnsNullWhenManagerUnbound) {
  mojo::PendingAssociatedRemote<mojom::PeerSessionManager> unbound_peer_manager;
  mojo::PendingAssociatedRemote<mojom::DesktopSessionManager>
      unbound_desktop_manager;
  IpcPeerSessionFactory factory(
      std::move(unbound_peer_manager), std::move(unbound_desktop_manager),
      base::BindRepeating([]() -> std::unique_ptr<protocol::IceConfigFetcher> {
        return std::make_unique<FakeIceConfigFetcher>(protocol::IceConfig());
      }));

  std::unique_ptr<PeerSession> session = factory.Create();
  EXPECT_EQ(session, nullptr);
}

TEST_F(IpcPeerSessionTest, IceConfigFetcherFetchesConfigOverMojo) {
  mojo::PendingRemote<mojom::PeerSession> remote;
  MockPeerSessionReceiver receiver;
  receiver.Bind(remote.InitWithNewPipeAndPassReceiver());

  protocol::IceConfig test_config;
  test_config.expiration_time = base::Time::Now() + base::Hours(1);
  test_config.max_bitrate_kbps = 1500;
  test_config.stun_servers.emplace_back("stun.example.com", 3478);
  test_config.turn_servers.emplace_back("turn.example.com", 3478, "test_user",
                                        "test_pass", webrtc::PROTO_UDP, false);

  auto session = std::make_unique<IpcPeerSession>(
      std::move(remote), base::DoNothing(),
      std::make_unique<FakeIceConfigFetcher>(test_config),
      base::NullCallback());

  MockEventHandler event_handler;
  base::RunLoop start_loop;
  receiver.set_start_closure(start_loop.QuitClosure());
  session->Start(&event_handler, "client@example.com/test",
                 DesktopEnvironmentOptions(), {}, SessionPolicies(),
                 SessionOptions());
  start_loop.Run();

  ASSERT_TRUE(receiver.start_called());
  ASSERT_TRUE(receiver.ice_config_fetcher_remote().is_bound());
  EXPECT_EQ(receiver.last_client_jid(), "client@example.com/test");

  base::RunLoop fetch_loop;
  std::optional<protocol::IceConfig> received_config;
  receiver.ice_config_fetcher_remote()->GetIceConfig(base::BindLambdaForTesting(
      [&](std::optional<protocol::IceConfig> config) {
        received_config = std::move(config);
        fetch_loop.Quit();
      }));
  fetch_loop.Run();

  ASSERT_TRUE(received_config.has_value());
  EXPECT_EQ(received_config->max_bitrate_kbps, 1500);
  ASSERT_EQ(received_config->stun_servers.size(), 1u);
  EXPECT_EQ(received_config->stun_servers[0].hostname(), "stun.example.com");
  EXPECT_EQ(received_config->stun_servers[0].port(), 3478);
  ASSERT_EQ(received_config->turn_servers.size(), 1u);
  EXPECT_EQ(received_config->turn_servers[0].credentials.username, "test_user");
  EXPECT_EQ(received_config->turn_servers[0].credentials.password, "test_pass");
}

TEST_F(IpcPeerSessionTest, PairingRequesterRequestsPairingOverMojo) {
  mojo::PendingRemote<mojom::PeerSession> remote;
  MockPeerSessionReceiver receiver;
  receiver.Bind(remote.InitWithNewPipeAndPassReceiver());

  std::string requested_client_name;
  auto request_pairing_cb = base::BindLambdaForTesting(
      [&](const std::string& client_name,
          PeerSessionFactory::RequestPairingResponseCallback response_cb) {
        requested_client_name = client_name;
        protocol::PairingResponse response;
        response.set_client_id("test_client_id_999");
        response.set_shared_secret("test_shared_secret_888");
        std::move(response_cb).Run(std::move(response));
      });

  auto session = std::make_unique<IpcPeerSession>(
      std::move(remote), base::DoNothing(),
      std::make_unique<FakeIceConfigFetcher>(protocol::IceConfig()),
      std::move(request_pairing_cb));

  MockEventHandler event_handler;
  base::RunLoop start_loop;
  receiver.set_start_closure(start_loop.QuitClosure());
  session->Start(&event_handler, "client@example.com/test",
                 DesktopEnvironmentOptions(), {}, SessionPolicies(),
                 SessionOptions());
  start_loop.Run();

  ASSERT_TRUE(receiver.start_called());
  ASSERT_TRUE(receiver.pairing_requester_remote().is_bound());

  base::RunLoop pairing_loop;
  std::optional<protocol::PairingResponse> received_response;
  receiver.pairing_requester_remote()->RequestPairing(
      "my_test_client",
      base::BindLambdaForTesting(
          [&](std::optional<protocol::PairingResponse> response) {
            received_response = std::move(response);
            pairing_loop.Quit();
          }));
  pairing_loop.Run();

  EXPECT_EQ(requested_client_name, "my_test_client");
  ASSERT_TRUE(received_response.has_value());
  EXPECT_EQ(received_response->client_id(), "test_client_id_999");
  EXPECT_EQ(received_response->shared_secret(), "test_shared_secret_888");
}

TEST_F(IpcPeerSessionTest, PairingRequesterReturnsNullWhenNoCallback) {
  mojo::PendingRemote<mojom::PeerSession> remote;
  MockPeerSessionReceiver receiver;
  receiver.Bind(remote.InitWithNewPipeAndPassReceiver());

  auto session = std::make_unique<IpcPeerSession>(
      std::move(remote), base::DoNothing(),
      std::make_unique<FakeIceConfigFetcher>(protocol::IceConfig()),
      base::NullCallback());

  MockEventHandler event_handler;
  base::RunLoop start_loop;
  receiver.set_start_closure(start_loop.QuitClosure());
  session->Start(&event_handler, "client@example.com/test",
                 DesktopEnvironmentOptions(), {}, SessionPolicies(),
                 SessionOptions());
  start_loop.Run();

  ASSERT_TRUE(receiver.start_called());
  ASSERT_TRUE(receiver.pairing_requester_remote().is_bound());

  base::RunLoop pairing_loop;
  std::optional<protocol::PairingResponse> received_response;
  receiver.pairing_requester_remote()->RequestPairing(
      "my_test_client",
      base::BindLambdaForTesting(
          [&](std::optional<protocol::PairingResponse> response) {
            received_response = std::move(response);
            pairing_loop.Quit();
          }));
  pairing_loop.Run();

  EXPECT_FALSE(received_response.has_value());
}

TEST_F(IpcPeerSessionTest, PairingRequesterRejectsSubsequentRequests) {
  mojo::PendingRemote<mojom::PeerSession> remote;
  MockPeerSessionReceiver receiver;
  receiver.Bind(remote.InitWithNewPipeAndPassReceiver());

  int call_count = 0;
  auto request_pairing_cb = base::BindLambdaForTesting(
      [&](const std::string& client_name,
          PeerSessionFactory::RequestPairingResponseCallback response_cb) {
        call_count++;
        std::move(response_cb).Run(protocol::PairingResponse());
      });

  auto session = std::make_unique<IpcPeerSession>(
      std::move(remote), base::DoNothing(),
      std::make_unique<FakeIceConfigFetcher>(protocol::IceConfig()),
      std::move(request_pairing_cb));

  MockEventHandler event_handler;
  base::RunLoop start_loop;
  receiver.set_start_closure(start_loop.QuitClosure());
  session->Start(&event_handler, "client@example.com/test",
                 DesktopEnvironmentOptions(), {}, SessionPolicies(),
                 SessionOptions());
  start_loop.Run();

  ASSERT_TRUE(receiver.start_called());
  ASSERT_TRUE(receiver.pairing_requester_remote().is_bound());

  base::RunLoop pairing_loop1;
  std::optional<protocol::PairingResponse> response1;
  receiver.pairing_requester_remote()->RequestPairing(
      "my_test_client",
      base::BindLambdaForTesting(
          [&](std::optional<protocol::PairingResponse> response) {
            response1 = std::move(response);
            pairing_loop1.Quit();
          }));
  pairing_loop1.Run();
  EXPECT_EQ(call_count, 1);
  EXPECT_TRUE(response1.has_value());

  // Calling RequestPairing a second time should return nullopt and not invoke
  // the callback.
  base::RunLoop pairing_loop2;
  std::optional<protocol::PairingResponse> response2 =
      protocol::PairingResponse();
  receiver.pairing_requester_remote()->RequestPairing(
      "my_test_client",
      base::BindLambdaForTesting(
          [&](std::optional<protocol::PairingResponse> response) {
            response2 = std::move(response);
            pairing_loop2.Quit();
          }));
  pairing_loop2.Run();
  EXPECT_EQ(call_count, 1);
  EXPECT_FALSE(response2.has_value());
}

TEST_F(IpcPeerSessionTest, IpcPeerSessionFactorySetsRequestPairingCallback) {
  mojo::AssociatedRemote<mojom::PeerSessionManager> peer_remote;
  mojo::PendingAssociatedReceiver<mojom::PeerSessionManager>
      peer_pending_receiver = peer_remote.BindNewEndpointAndPassReceiver();
  peer_pending_receiver.EnableUnassociatedUsage();

  mojo::AssociatedRemote<mojom::DesktopSessionManager> desktop_remote;
  mojo::PendingAssociatedReceiver<mojom::DesktopSessionManager>
      desktop_pending_receiver =
          desktop_remote.BindNewEndpointAndPassReceiver();
  desktop_pending_receiver.EnableUnassociatedUsage();

  MockPeerSessionManager mock_peer_manager;
  mock_peer_manager.Bind(std::move(peer_pending_receiver));

  MockDesktopSessionManager mock_desktop_manager;
  mock_desktop_manager.Bind(std::move(desktop_pending_receiver));

  IpcPeerSessionFactory factory(
      peer_remote.Unbind(), desktop_remote.Unbind(),
      base::BindRepeating([]() -> std::unique_ptr<protocol::IceConfigFetcher> {
        return std::make_unique<FakeIceConfigFetcher>(protocol::IceConfig());
      }));

  bool pairing_called = false;
  factory.set_request_pairing_callback(base::BindLambdaForTesting(
      [&](const std::string& client_name,
          PeerSessionFactory::RequestPairingResponseCallback response_cb) {
        pairing_called = true;
        std::move(response_cb).Run(protocol::PairingResponse());
      }));

  std::unique_ptr<PeerSession> session = factory.Create();
  ASSERT_NE(session, nullptr);

  static_cast<IpcPeerSession*>(session.get())
      ->RequestPairing("test_client", base::DoNothing());
  EXPECT_TRUE(pairing_called);
}

}  // namespace remoting
