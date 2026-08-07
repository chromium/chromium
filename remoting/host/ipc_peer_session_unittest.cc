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
#include "remoting/protocol/errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

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

  bool is_bound() const { return receiver_.is_bound(); }

  // mojom::PeerSession implementation:
  void Start(
      mojo::PendingRemote<mojom::PeerSessionEventHandler> event_handler,
      const std::string& client_jid,
      mojo::PendingRemote<mojom::DesktopSession> control_remote,
      mojo::PendingReceiver<mojom::DesktopSessionEvents> events_receiver,
      const DesktopEnvironmentOptions& desktop_environment_options) override {}
  void DisconnectSession(protocol::ErrorCode error,
                         const std::string& error_details,
                         const SourceLocation& error_location) override {}
  void OnSessionServicesClientConnected(
      mojo::PendingReceiver<mojom::ChromotingSessionServices> receiver)
      override {}

 private:
  mojo::Receiver<mojom::PeerSession> receiver_{this};
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

  auto session =
      std::make_unique<IpcPeerSession>(std::move(remote), base::DoNothing());
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

  auto session =
      std::make_unique<IpcPeerSession>(std::move(remote), base::DoNothing());
  MockEventHandler event_handler;
  session->Start(&event_handler, "", DesktopEnvironmentOptions(), {},
                 SessionPolicies(), SessionOptions());
  session->DisconnectSession(protocol::ErrorCode::OK, "", FROM_HERE);
  EXPECT_EQ(session->transport(), nullptr);
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

  IpcPeerSessionFactory factory(peer_remote.Unbind(), desktop_remote.Unbind());

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

  IpcPeerSessionFactory factory(peer_remote.Unbind(), desktop_remote.Unbind());
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

TEST_F(IpcPeerSessionTest, CreateReturnsNullWhenManagerUnbound) {
  mojo::PendingAssociatedRemote<mojom::PeerSessionManager> unbound_peer_manager;
  mojo::PendingAssociatedRemote<mojom::DesktopSessionManager>
      unbound_desktop_manager;
  IpcPeerSessionFactory factory(std::move(unbound_peer_manager),
                                std::move(unbound_desktop_manager));

  std::unique_ptr<PeerSession> session = factory.Create();
  EXPECT_EQ(session, nullptr);
}

}  // namespace remoting
