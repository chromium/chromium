// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromoting_host_services_client.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/environment.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "remoting/host/mojo_ipc/mojo_ipc_server.h"
#include "remoting/host/mojo_ipc/mojo_ipc_test_util.h"
#include "remoting/host/mojom/chromoting_host_services.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

class ChromotingHostServicesClientTest : public testing::Test,
                                         public mojom::ChromotingHostServices {
 public:
  ChromotingHostServicesClientTest();
  ~ChromotingHostServicesClientTest() override;

  void BindSessionServices(
      mojo::PendingReceiver<mojom::ChromotingSessionServices> receiver)
      override;

 protected:
  void SetChromeRemoteDesktopSessionEnvVar(bool is_crd_session);
  void WaitForInvitationSent();
  void WaitForSessionServicesBound();
  void SetRemoteDisconnectCallback(base::OnceClosure callback);

  std::unique_ptr<ChromotingHostServicesClient> client_;
  std::unique_ptr<MojoIpcServer<mojom::ChromotingHostServices>> ipc_server_;
  std::vector<mojo::PendingReceiver<mojom::ChromotingSessionServices>>
      receivers_;

 private:
  void OnInvitationSent();

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  raw_ptr<base::Environment> environment_;

  // Used to block the thread until the server has sent out an invitation.
  std::unique_ptr<base::RunLoop> on_invitation_sent_run_loop_;

  // Used to block the thread until a session services bind request is received.
  std::unique_ptr<base::RunLoop> session_services_bound_run_loop_;
};

ChromotingHostServicesClientTest::ChromotingHostServicesClientTest() {
  auto test_server_name = test::GenerateRandomServerName();
  auto environment = base::Environment::Create();
  environment_ = environment.get();
  client_ = base::WrapUnique(new ChromotingHostServicesClient(
      std::move(environment), test_server_name));
  ipc_server_ = std::make_unique<MojoIpcServer<mojom::ChromotingHostServices>>(
      test_server_name, this);
  ipc_server_->set_on_invitation_sent_callback_for_testing(
      base::BindRepeating(&ChromotingHostServicesClientTest::OnInvitationSent,
                          base::Unretained(this)));
  on_invitation_sent_run_loop_ = std::make_unique<base::RunLoop>();
  session_services_bound_run_loop_ = std::make_unique<base::RunLoop>();
  SetChromeRemoteDesktopSessionEnvVar(true);
}

ChromotingHostServicesClientTest::~ChromotingHostServicesClientTest() = default;

void ChromotingHostServicesClientTest::BindSessionServices(
    mojo::PendingReceiver<mojom::ChromotingSessionServices> receiver) {
  receivers_.push_back(std::move(receiver));
  session_services_bound_run_loop_->Quit();
}

void ChromotingHostServicesClientTest::SetChromeRemoteDesktopSessionEnvVar(
    bool is_crd_session) {
#if BUILDFLAG(IS_LINUX)
  if (is_crd_session) {
    environment_->SetVar(
        ChromotingHostServicesClient::kChromeRemoteDesktopSessionEnvVar, "1");
  } else {
    environment_->UnSetVar(
        ChromotingHostServicesClient::kChromeRemoteDesktopSessionEnvVar);
  }
#endif
  // No-op on other platforms.
}

void ChromotingHostServicesClientTest::WaitForInvitationSent() {
  on_invitation_sent_run_loop_->Run();
  on_invitation_sent_run_loop_ = std::make_unique<base::RunLoop>();
}

void ChromotingHostServicesClientTest::WaitForSessionServicesBound() {
  session_services_bound_run_loop_->Run();
  session_services_bound_run_loop_ = std::make_unique<base::RunLoop>();
}

void ChromotingHostServicesClientTest::SetRemoteDisconnectCallback(
    base::OnceClosure callback) {
  client_->on_session_disconnected_callback_for_testing_ = std::move(callback);
}

void ChromotingHostServicesClientTest::OnInvitationSent() {
  on_invitation_sent_run_loop_->Quit();
}

TEST_F(ChromotingHostServicesClientTest,
       ServerNotRunning_GetSessionServicesReturnsNull) {
  ipc_server_->StopServer();
  ASSERT_EQ(client_->GetSessionServices(), nullptr);
}

#if BUILDFLAG(IS_LINUX)

TEST_F(ChromotingHostServicesClientTest,
       NotInRemoteDesktopSession_GetSessionServicesReturnsNull) {
  SetChromeRemoteDesktopSessionEnvVar(false);
  ipc_server_->StartServer();
  WaitForInvitationSent();
  ASSERT_EQ(client_->GetSessionServices(), nullptr);
}

#endif

TEST_F(ChromotingHostServicesClientTest,
       CallGetSessionServicesTwice_SamePointerReturned) {
  ipc_server_->StartServer();
  WaitForInvitationSent();
  auto* session_services = client_->GetSessionServices();
  ASSERT_NE(session_services, nullptr);
  WaitForSessionServicesBound();
  ASSERT_EQ(receivers_.size(), 1u);
  ASSERT_EQ(client_->GetSessionServices(), session_services);
}

TEST_F(ChromotingHostServicesClientTest,
       ServerClosesReceiverAndClientReconnects) {
  ipc_server_->StartServer();
  WaitForInvitationSent();
  ASSERT_NE(client_->GetSessionServices(), nullptr);
  WaitForSessionServicesBound();
  ASSERT_EQ(receivers_.size(), 1u);

  base::RunLoop remote_disconnect_run_loop;
  SetRemoteDisconnectCallback(remote_disconnect_run_loop.QuitClosure());
  receivers_.clear();
  remote_disconnect_run_loop.Run();

  ASSERT_NE(client_->GetSessionServices(), nullptr);
  WaitForSessionServicesBound();
  ASSERT_EQ(receivers_.size(), 1u);
}

// Ideally we should also verify that the client can reconnect after the server
// is restarted. This doesn't seem to work with single process unit test though,
// since mojo for some reason can't cleanly tear down a connection when both
// ends of the platform channel are from the same process. This issue doesn't
// seem to happen in the real world, where platform channels are always used
// between two processes.
// TODO(yuweih): Consider adding a test to verify this by spawning a separate
// server process.

}  // namespace remoting
