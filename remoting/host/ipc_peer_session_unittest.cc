// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ipc_peer_session.h"

#include <memory>
#include <utility>
#include <vector>

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

  auto session = std::make_unique<IpcPeerSession>(std::move(remote));
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

  auto session = std::make_unique<IpcPeerSession>(std::move(remote));
  session->Start(nullptr, "", DesktopEnvironmentOptions(), {},
                 SessionPolicies(), SessionOptions());
  session->DisconnectSession(protocol::ErrorCode::OK, "", FROM_HERE);
  EXPECT_EQ(session->transport(), nullptr);
}

TEST_F(IpcPeerSessionTest, CreateInvokesLaunchPeerSession) {
  mojo::AssociatedRemote<mojom::PeerSessionManager> remote;
  mojo::PendingAssociatedReceiver<mojom::PeerSessionManager> pending_receiver =
      remote.BindNewEndpointAndPassReceiver();
  pending_receiver.EnableUnassociatedUsage();

  MockPeerSessionManager mock_manager;
  mock_manager.Bind(std::move(pending_receiver));

  IpcPeerSessionFactory factory(remote.Unbind());

  base::RunLoop run_loop;
  mock_manager.set_quit_closure(run_loop.QuitClosure());

  std::unique_ptr<PeerSession> session = factory.Create();
  EXPECT_NE(session, nullptr);

  run_loop.Run();
  EXPECT_TRUE(mock_manager.launch_called());
}

TEST_F(IpcPeerSessionTest, CreateReturnsNullWhenManagerUnbound) {
  mojo::PendingAssociatedRemote<mojom::PeerSessionManager> unbound_manager;
  IpcPeerSessionFactory factory(std::move(unbound_manager));

  std::unique_ptr<PeerSession> session = factory.Create();
  EXPECT_EQ(session, nullptr);
}

}  // namespace remoting
