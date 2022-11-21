// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromoting_host.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/named_mojo_ipc_server/fake_ipc_server.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/network_change_notifier.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/fake_desktop_environment.h"
#include "remoting/host/fake_mouse_cursor_monitor.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/host/mojom/chromoting_host_services.mojom.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/errors.h"
#include "remoting/protocol/fake_connection_to_client.h"
#include "remoting/protocol/fake_desktop_capturer.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "remoting/protocol/session_config.h"
#include "remoting/protocol/transport_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

using ::remoting::protocol::MockClientStub;
using ::remoting::protocol::MockConnectionToClientEventHandler;
using ::remoting::protocol::MockHostStub;
using ::remoting::protocol::MockSession;
using ::remoting::protocol::MockVideoStub;
using ::remoting::protocol::Session;
using ::remoting::protocol::SessionConfig;

using testing::_;
using testing::AnyNumber;
using testing::AtLeast;
using testing::AtMost;
using testing::DeleteArg;
using testing::DoAll;
using testing::Expectation;
using testing::InSequence;
using testing::Invoke;
using testing::InvokeArgument;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::ReturnRef;
using testing::SaveArg;
using testing::Sequence;

namespace remoting {

const size_t kNumFailuresIgnored = 5;

class ChromotingHostTest : public testing::Test {
 public:
  ChromotingHostTest() = default;

  void SetUp() override {
    network_change_notifier_ = net::NetworkChangeNotifier::CreateIfNeeded();

    task_runner_ = new AutoThreadTaskRunner(
        base::SingleThreadTaskRunner::GetCurrentDefault(), base::DoNothing());

    desktop_environment_factory_ =
        std::make_unique<FakeDesktopEnvironmentFactory>(
            base::SingleThreadTaskRunner::GetCurrentDefault());
    session_manager_ = new protocol::MockSessionManager();

    host_ = std::make_unique<ChromotingHost>(
        desktop_environment_factory_.get(),
        base::WrapUnique(session_manager_.get()),
        protocol::TransportContext::ForTests(protocol::TransportRole::SERVER),
        task_runner_,  // Audio
        task_runner_,
        DesktopEnvironmentOptions::CreateDefault());  // Video encode
    host_->status_monitor()->AddStatusObserver(&host_status_observer_);

    owner_email_ = "host@domain";
    session1_ = new MockSession();
    session2_ = new MockSession();
    session_unowned1_ = std::make_unique<MockSession>();
    session_unowned2_ = std::make_unique<MockSession>();
    session_config1_ = SessionConfig::ForTest();
    session_jid1_ = "user@domain/rest-of-jid";
    session_config2_ = SessionConfig::ForTest();
    session_jid2_ = "user2@domain/rest-of-jid";
    session_unowned_jid1_ = "user3@doman/rest-of-jid";
    session_unowned_jid2_ = "user4@doman/rest-of-jid";

    EXPECT_CALL(*session1_, jid()).WillRepeatedly(ReturnRef(session_jid1_));
    EXPECT_CALL(*session2_, jid()).WillRepeatedly(ReturnRef(session_jid2_));
    EXPECT_CALL(*session_unowned1_, jid())
        .WillRepeatedly(ReturnRef(session_unowned_jid1_));
    EXPECT_CALL(*session_unowned2_, jid())
        .WillRepeatedly(ReturnRef(session_unowned_jid2_));
    EXPECT_CALL(*session_unowned1_, SetEventHandler(_))
        .Times(AnyNumber())
        .WillRepeatedly(SaveArg<0>(&session_unowned1_event_handler_));
    EXPECT_CALL(*session_unowned2_, SetEventHandler(_))
        .Times(AnyNumber())
        .WillRepeatedly(SaveArg<0>(&session_unowned2_event_handler_));
    EXPECT_CALL(*session1_, config())
        .WillRepeatedly(ReturnRef(*session_config1_));
    EXPECT_CALL(*session2_, config())
        .WillRepeatedly(ReturnRef(*session_config2_));
    EXPECT_CALL(*session_unowned1_, config())
        .WillRepeatedly(ReturnRef(*session_config1_));
    EXPECT_CALL(*session_unowned2_, config())
        .WillRepeatedly(ReturnRef(*session_config2_));

    owned_connection1_ = std::make_unique<protocol::FakeConnectionToClient>(
        base::WrapUnique(session1_.get()));
    owned_connection1_->set_host_stub(&host_stub1_);
    connection1_ = owned_connection1_.get();
    connection1_->set_client_stub(&client_stub1_);

    owned_connection2_ = std::make_unique<protocol::FakeConnectionToClient>(
        base::WrapUnique(session2_.get()));
    owned_connection2_->set_host_stub(&host_stub2_);
    connection2_ = owned_connection2_.get();
    connection2_->set_client_stub(&client_stub2_);
  }

  // Helper method to pretend a client is connected to ChromotingHost.
  void SimulateClientConnection(int connection_index,
                                bool authenticate,
                                bool reject) {
    std::unique_ptr<protocol::ConnectionToClient> connection = std::move(
        (connection_index == 0) ? owned_connection1_ : owned_connection2_);
    protocol::ConnectionToClient* connection_ptr = connection.get();
    std::unique_ptr<ClientSession> client(new ClientSession(
        host_.get(), std::move(connection), desktop_environment_factory_.get(),
        DesktopEnvironmentOptions::CreateDefault(), base::TimeDelta(), nullptr,
        std::vector<HostExtension*>()));
    ClientSession* client_ptr = client.get();

    connection_ptr->set_host_stub(client.get());
    get_client(connection_index) = client_ptr;

    // |host| is responsible for deleting |client| from now on.
    host_->clients_.push_back(std::move(client));

    if (authenticate) {
      client_ptr->OnConnectionAuthenticated();
      if (!reject)
        client_ptr->OnConnectionChannelsConnected();
    } else {
      client_ptr->OnConnectionClosed(protocol::AUTHENTICATION_FAILED);
    }
  }

  void TearDown() override {
    if (host_)
      ShutdownHost();
    task_runner_ = nullptr;

    base::RunLoop().RunUntilIdle();
  }

  void NotifyConnectionClosed1() {
    if (session_unowned1_event_handler_) {
      session_unowned1_event_handler_->OnSessionStateChange(Session::CLOSED);
    }
  }

  void NotifyConnectionClosed2() {
    if (session_unowned2_event_handler_) {
      session_unowned2_event_handler_->OnSessionStateChange(Session::CLOSED);
    }
  }

  void ShutdownHost() {
    EXPECT_CALL(host_status_observer_, OnHostShutdown());
    host_.reset();
    desktop_environment_factory_.reset();
  }

  // Starts the host.
  void StartHost() {
    EXPECT_CALL(host_status_observer_, OnHostStarted(owner_email_));
    EXPECT_CALL(*session_manager_, AcceptIncoming(_));
    host_->Start(owner_email_);
  }

  // Expect a client to connect.
  // Return an expectation that a session has started.
  Expectation ExpectClientConnected(int connection_index) {
    const std::string& session_jid = get_session_jid(connection_index);

    Expectation client_authenticated =
        EXPECT_CALL(host_status_observer_, OnClientAuthenticated(session_jid));
    return EXPECT_CALL(host_status_observer_, OnClientConnected(session_jid))
        .After(client_authenticated);
  }

  // Expect that a client is disconnected. The given action will be done after
  // the status observer is notified that the session has finished.
  Expectation ExpectClientDisconnected(int connection_index) {
    return EXPECT_CALL(host_status_observer_,
                       OnClientDisconnected(get_session_jid(connection_index)))
        .RetiresOnSaturation();
  }

  void StartFakeIpcServer() {
    host_->ipc_server_ = std::make_unique<named_mojo_ipc_server::FakeIpcServer>(
        &ipc_server_test_state_);
    host_->ipc_server_->StartServer();
  }

#if BUILDFLAG(IS_WIN)
  // Simulates the IPC client's session ID for the session ID check in
  // ChromotingHost::BindSessionServices.
  //
  // |is_remote_desktop_session_id|: True if the simulated session ID should be
  // exactly the session ID of the fake desktop environment. If false, the
  // simulated session ID is guaranteed to be different from the desktop
  // environment's session ID.
  void SimulateIpcClientSessionId(bool is_remote_desktop_session_id) {
    // ChromotingHost::BindSessionServices calls ProcessIdToSessionId() on the
    // IPC client's PID. The PID we know that always works is the current
    // process' PID.
    auto current_pid = base::GetCurrentProcId();
    ipc_server_test_state_.current_peer_pid = current_pid;
    DWORD current_session_id;
    bool success = ProcessIdToSessionId(current_pid, &current_session_id);
    ASSERT_TRUE(success);
    // The IPC client's session ID is exactly the current process' session ID
    // at this point, so we change the fake desktop environment's session ID
    // here.
    if (is_remote_desktop_session_id) {
      desktop_environment_factory_->set_desktop_session_id(current_session_id);
    } else {
      desktop_environment_factory_->set_desktop_session_id(current_session_id +
                                                           1);
    }
  }
#endif

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  scoped_refptr<AutoThreadTaskRunner> task_runner_;
  MockConnectionToClientEventHandler handler_;
  std::unique_ptr<FakeDesktopEnvironmentFactory> desktop_environment_factory_;
  MockHostStatusObserver host_status_observer_;
  std::unique_ptr<ChromotingHost> host_;
  raw_ptr<protocol::MockSessionManager> session_manager_;
  std::string owner_email_;
  raw_ptr<protocol::FakeConnectionToClient> connection1_;
  std::unique_ptr<protocol::FakeConnectionToClient> owned_connection1_;
  ClientSession* client1_;
  std::string session_jid1_;
  raw_ptr<MockSession> session1_;  // Owned by |connection_|.
  std::unique_ptr<SessionConfig> session_config1_;
  MockClientStub client_stub1_;
  MockHostStub host_stub1_;
  raw_ptr<protocol::FakeConnectionToClient> connection2_;
  std::unique_ptr<protocol::FakeConnectionToClient> owned_connection2_;
  ClientSession* client2_;
  std::string session_jid2_;
  raw_ptr<MockSession> session2_;  // Owned by |connection2_|.
  std::unique_ptr<SessionConfig> session_config2_;
  MockClientStub client_stub2_;
  MockHostStub host_stub2_;
  std::unique_ptr<MockSession> session_unowned1_;  // Not owned by a connection.
  std::string session_unowned_jid1_;
  std::unique_ptr<MockSession> session_unowned2_;  // Not owned by a connection.
  std::string session_unowned_jid2_;
  protocol::Session::EventHandler* session_unowned1_event_handler_;
  protocol::Session::EventHandler* session_unowned2_event_handler_;
  named_mojo_ipc_server::FakeIpcServer::TestState ipc_server_test_state_;

  // Returns the cached client pointers client1_ or client2_.
  ClientSession*& get_client(int connection_index) {
    return (connection_index == 0) ? client1_ : client2_;
  }

  const std::string& get_session_jid(int connection_index) {
    return (connection_index == 0) ? session_jid1_ : session_jid2_;
  }
};

TEST_F(ChromotingHostTest, StartAndShutdown) {
  StartHost();
}

TEST_F(ChromotingHostTest, Connect) {
  StartHost();

  // Shut down the host when the first video packet is received.
  ExpectClientConnected(0);
  SimulateClientConnection(0, true, false);
}

TEST_F(ChromotingHostTest, AuthenticationFailed) {
  StartHost();

  EXPECT_CALL(host_status_observer_, OnClientAccessDenied(session_jid1_));
  SimulateClientConnection(0, false, false);
}

TEST_F(ChromotingHostTest, Reconnect) {
  StartHost();

  // Connect first client.
  ExpectClientConnected(0);
  SimulateClientConnection(0, true, false);

  // Disconnect first client.
  ExpectClientDisconnected(0);
  client1_->OnConnectionClosed(protocol::OK);

  // Connect second client.
  ExpectClientConnected(1);
  SimulateClientConnection(1, true, false);

  // Disconnect second client.
  ExpectClientDisconnected(1);
  client2_->OnConnectionClosed(protocol::OK);
}

TEST_F(ChromotingHostTest, ConnectWhenAnotherClientIsConnected) {
  StartHost();

  // Connect first client.
  ExpectClientConnected(0);
  SimulateClientConnection(0, true, false);

  // Connect second client. First client should be disconnected automatically.
  {
    InSequence s;
    ExpectClientDisconnected(0);
    ExpectClientConnected(1);
  }
  SimulateClientConnection(1, true, false);

  // Disconnect second client.
  ExpectClientDisconnected(1);
  client2_->OnConnectionClosed(protocol::OK);
}

TEST_F(ChromotingHostTest, IncomingSessionAccepted) {
  StartHost();

  MockSession* session = session_unowned1_.get();
  protocol::SessionManager::IncomingSessionResponse response =
      protocol::SessionManager::DECLINE;
  host_->OnIncomingSession(session_unowned1_.release(), &response);
  EXPECT_EQ(protocol::SessionManager::ACCEPT, response);

  EXPECT_CALL(*session, Close(_))
      .WillOnce(InvokeWithoutArgs(
          this, &ChromotingHostTest::NotifyConnectionClosed1));
  ShutdownHost();
}

TEST_F(ChromotingHostTest, LoginBackOffTriggersIfClientsDoNotAuthenticate) {
  StartHost();

  protocol::SessionManager::IncomingSessionResponse response =
      protocol::SessionManager::DECLINE;
  protocol::Session::EventHandler*
      session_event_handlers[kNumFailuresIgnored + 1];
  for (size_t i = 0; i < kNumFailuresIgnored + 1; ++i) {
    // Set expectations and responses for the new session.
    auto session = std::make_unique<MockSession>();
    EXPECT_CALL(*session, jid()).WillRepeatedly(ReturnRef(session_jid1_));
    EXPECT_CALL(*session, config())
        .WillRepeatedly(ReturnRef(*session_config1_));
    EXPECT_CALL(*session, SetEventHandler(_))
        .Times(AnyNumber())
        .WillRepeatedly(SaveArg<0>(&session_event_handlers[i]));
    EXPECT_CALL(*session, Close(_))
        .WillOnce(InvokeWithoutArgs([&session_event_handlers, i]() {
          session_event_handlers[i]->OnSessionStateChange(Session::CLOSED);
        }));
    // Simulate the incoming connection.
    host_->OnIncomingSession(session.release(), &response);
    EXPECT_EQ(protocol::SessionManager::ACCEPT, response);
    // Begin authentication; this will increase the backoff count, and since
    // OnSessionAuthenticated is never called, the host should only allow
    // kNumFailuresIgnored + 1 connections before beginning the backoff.
    host_->OnSessionAuthenticating(
        host_->client_sessions_for_tests().front().get());
  }

  // As this is connection kNumFailuresIgnored + 2, it should be rejected.
  host_->OnIncomingSession(session_unowned2_.get(), &response);
  EXPECT_EQ(protocol::SessionManager::OVERLOAD, response);
  EXPECT_EQ(host_->client_sessions_for_tests().size(), kNumFailuresIgnored + 1);

  // Shut down host while objects owned by this test are still in scope.
  ShutdownHost();
}

TEST_F(ChromotingHostTest, LoginBackOffResetsIfClientsAuthenticate) {
  StartHost();

  protocol::SessionManager::IncomingSessionResponse response =
      protocol::SessionManager::DECLINE;
  protocol::Session::EventHandler*
      session_event_handlers[kNumFailuresIgnored + 1];
  for (size_t i = 0; i < kNumFailuresIgnored + 1; ++i) {
    // Set expectations and responses for the new session.
    auto session = std::make_unique<MockSession>();
    EXPECT_CALL(*session, jid()).WillRepeatedly(ReturnRef(session_jid1_));
    EXPECT_CALL(*session, config())
        .WillRepeatedly(ReturnRef(*session_config1_));
    EXPECT_CALL(*session, SetEventHandler(_))
        .Times(AnyNumber())
        .WillRepeatedly(SaveArg<0>(&session_event_handlers[i]));
    EXPECT_CALL(*session, Close(_))
        .WillOnce(InvokeWithoutArgs([&session_event_handlers, i]() {
          session_event_handlers[i]->OnSessionStateChange(Session::CLOSED);
        }));
    // Simulate the incoming connection.
    host_->OnIncomingSession(session.release(), &response);
    EXPECT_EQ(protocol::SessionManager::ACCEPT, response);
    // Begin authentication; this will increase the backoff count
    host_->OnSessionAuthenticating(
        host_->client_sessions_for_tests().front().get());
  }

  // Simulate successful authentication for one of the previous connections.
  // This should reset the backoff and disconnect all the other connections.
  host_->OnSessionAuthenticated(
      host_->client_sessions_for_tests().front().get());
  EXPECT_EQ(host_->client_sessions_for_tests().size(), 1U);

  // This is connection kNumFailuresIgnored + 2, but since we now have a
  // successful authentication it should not be rejected.
  auto session = std::make_unique<MockSession>();
  protocol::Session::EventHandler* session_event_handler;
  EXPECT_CALL(*session, jid()).WillRepeatedly(ReturnRef(session_jid1_));
  EXPECT_CALL(*session, config()).WillRepeatedly(ReturnRef(*session_config1_));
  EXPECT_CALL(*session, SetEventHandler(_))
      .Times(AnyNumber())
      .WillRepeatedly(SaveArg<0>(&session_event_handler));
  EXPECT_CALL(*session, Close(_))
      .WillOnce(InvokeWithoutArgs([&session_event_handler]() {
        session_event_handler->OnSessionStateChange(Session::CLOSED);
      }));
  host_->OnIncomingSession(session.release(), &response);
  EXPECT_EQ(protocol::SessionManager::ACCEPT, response);

  // Shut down host while objects owned by this test are still in scope.
  ShutdownHost();
}

// Flaky on all platforms.  http://crbug.com/1265894
TEST_F(ChromotingHostTest, DISABLED_OnSessionRouteChange) {
  StartHost();

  ExpectClientConnected(0);
  SimulateClientConnection(0, true, false);

  std::string channel_name("ChannelName");
  protocol::TransportRoute route;
  EXPECT_CALL(host_status_observer_,
              OnClientRouteChange(session_jid1_, channel_name, _));
  host_->OnSessionRouteChange(get_client(0), channel_name, route);
}

TEST_F(ChromotingHostTest, BindSessionServicesWithNoConnectedSession_Rejected) {
  StartHost();

  mojo::Remote<mojom::ChromotingSessionServices> remote;
  auto receiver = remote.BindNewPipeAndPassReceiver();
  base::RunLoop wait_for_disconnect_run_loop;
  remote.set_disconnect_handler(wait_for_disconnect_run_loop.QuitClosure());
  host_->BindSessionServices(std::move(receiver));
  wait_for_disconnect_run_loop.Run();
}

TEST_F(ChromotingHostTest, BindSessionServicesWithConnectedSession_Accepted) {
  StartHost();
  StartFakeIpcServer();
#if BUILDFLAG(IS_WIN)
  SimulateIpcClientSessionId(/* is_remote_desktop_session_id= */ true);
#endif
  ExpectClientConnected(0);
  SimulateClientConnection(0, true, false);

  mojo::Remote<mojom::ChromotingSessionServices> remote;
  auto receiver = remote.BindNewPipeAndPassReceiver();
  base::RunLoop wait_for_version_run_loop;
  remote.set_disconnect_handler(base::BindLambdaForTesting([&]() {
    wait_for_version_run_loop.Quit();
    FAIL() << "Disconnect handler should not be called.";
  }));
  // QueryVersion() is used to determine whether the server accepts the bind
  // request; if it doesn't, the callback won't be called, and the disconnect
  // handler will be called instead.
  remote.QueryVersion(base::BindLambdaForTesting(
      [&](uint32_t version) { wait_for_version_run_loop.Quit(); }));
  host_->BindSessionServices(std::move(receiver));
  wait_for_version_run_loop.Run();
}

#if BUILDFLAG(IS_WIN)
TEST_F(ChromotingHostTest, BindSessionServicesWithWrongSession_Rejected) {
  StartHost();
  StartFakeIpcServer();
  SimulateIpcClientSessionId(/* is_remote_desktop_session_id= */ false);
  ExpectClientConnected(0);
  SimulateClientConnection(0, true, false);

  mojo::Remote<mojom::ChromotingSessionServices> remote;
  auto receiver = remote.BindNewPipeAndPassReceiver();
  base::RunLoop wait_for_disconnect_run_loop;
  remote.set_disconnect_handler(wait_for_disconnect_run_loop.QuitClosure());
  host_->BindSessionServices(std::move(receiver));
  wait_for_disconnect_run_loop.Run();
}
#endif

}  // namespace remoting
