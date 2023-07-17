// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromoting_host_services_client.h"

#include <memory>
#include <vector>

#include "base/environment.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/system/isolated_connection.h"
#include "remoting/host/mojo_caller_security_checker.h"
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

  void TearDown() override;

 protected:
  void SetChromeRemoteDesktopSessionEnvVar(bool is_crd_session);
  void WaitForSessionServicesBound();
  void SetRemoteDisconnectCallback(base::OnceClosure callback);

  base::test::TaskEnvironment task_environment_;
  raw_ptr<base::Environment, DanglingUntriaged> environment_;
  bool is_server_started_ = true;
  std::unique_ptr<ChromotingHostServicesClient> client_;
  mojo::ReceiverSet<mojom::ChromotingHostServices> host_services_receivers_;
  std::vector<mojo::PendingReceiver<mojom::ChromotingSessionServices>>
      session_services_receivers_;

 private:
  mojo::PendingRemote<mojom::ChromotingHostServices> ConnectToServer();

  // Used to block the thread until a session services bind request is received.
  std::unique_ptr<base::RunLoop> session_services_bound_run_loop_;
};

ChromotingHostServicesClientTest::ChromotingHostServicesClientTest() {
  auto environment = base::Environment::Create();
  environment_ = environment.get();
  client_ = base::WrapUnique(new ChromotingHostServicesClient(
      std::move(environment),
      base::BindRepeating(&ChromotingHostServicesClientTest::ConnectToServer,
                          base::Unretained(this))));
  session_services_bound_run_loop_ = std::make_unique<base::RunLoop>();
  SetChromeRemoteDesktopSessionEnvVar(true);
}

ChromotingHostServicesClientTest::~ChromotingHostServicesClientTest() = default;

void ChromotingHostServicesClientTest::BindSessionServices(
    mojo::PendingReceiver<mojom::ChromotingSessionServices> receiver) {
  session_services_receivers_.push_back(std::move(receiver));
  session_services_bound_run_loop_->Quit();
}

void ChromotingHostServicesClientTest::TearDown() {
  task_environment_.RunUntilIdle();
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

void ChromotingHostServicesClientTest::WaitForSessionServicesBound() {
  session_services_bound_run_loop_->Run();
  session_services_bound_run_loop_ = std::make_unique<base::RunLoop>();
}

void ChromotingHostServicesClientTest::SetRemoteDisconnectCallback(
    base::OnceClosure callback) {
  client_->on_session_disconnected_callback_for_testing_ = std::move(callback);
}

mojo::PendingRemote<mojom::ChromotingHostServices>
ChromotingHostServicesClientTest::ConnectToServer() {
  if (!is_server_started_) {
    return mojo::PendingRemote<mojom::ChromotingHostServices>();
  }
  mojo::PendingReceiver<mojom::ChromotingHostServices> pending_receiver;
  auto pending_remote = pending_receiver.InitWithNewPipeAndPassRemote();
  host_services_receivers_.Add(this, std::move(pending_receiver));
  return pending_remote;
}

TEST_F(ChromotingHostServicesClientTest,
       ServerNotRunning_GetSessionServicesReturnsNull) {
  is_server_started_ = false;
  ASSERT_EQ(client_->GetSessionServices(), nullptr);
}

#if BUILDFLAG(IS_LINUX)

TEST_F(ChromotingHostServicesClientTest,
       NotInRemoteDesktopSession_GetSessionServicesReturnsNull) {
  SetChromeRemoteDesktopSessionEnvVar(false);
  ASSERT_EQ(client_->GetSessionServices(), nullptr);
}

#endif

TEST_F(ChromotingHostServicesClientTest,
       CallGetSessionServicesTwice_SamePointerReturned) {
  auto* session_services = client_->GetSessionServices();
  ASSERT_NE(session_services, nullptr);
  ASSERT_EQ(host_services_receivers_.size(), 1u);
  WaitForSessionServicesBound();
  ASSERT_EQ(session_services_receivers_.size(), 1u);
  ASSERT_EQ(client_->GetSessionServices(), session_services);
  ASSERT_EQ(host_services_receivers_.size(), 1u);
  ASSERT_EQ(session_services_receivers_.size(), 1u);
}

TEST_F(ChromotingHostServicesClientTest,
       ServerClosesReceiverAndClientReconnects) {
  ASSERT_NE(client_->GetSessionServices(), nullptr);
  WaitForSessionServicesBound();
  ASSERT_EQ(session_services_receivers_.size(), 1u);

  base::RunLoop remote_disconnect_run_loop;
  SetRemoteDisconnectCallback(remote_disconnect_run_loop.QuitClosure());
  session_services_receivers_.clear();
  remote_disconnect_run_loop.Run();

  ASSERT_NE(client_->GetSessionServices(), nullptr);
  WaitForSessionServicesBound();
  ASSERT_EQ(session_services_receivers_.size(), 1u);
}

}  // namespace remoting
