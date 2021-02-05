// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/ash/camera_hal_dispatcher_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/posix/safe_strerror.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/task_environment.h"
#include "media/capture/video/chromeos/mojom/camera_common.mojom.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::InvokeWithoutArgs;

namespace media {
namespace {

class MockCameraHalServer : public cros::mojom::CameraHalServer {
 public:
  MockCameraHalServer() = default;

  ~MockCameraHalServer() = default;

  void CreateChannel(
      mojo::PendingReceiver<cros::mojom::CameraModule> camera_module_receiver,
      cros::mojom::CameraClientType camera_client_type) override {
    DoCreateChannel(std::move(camera_module_receiver), camera_client_type);
  }
  MOCK_METHOD2(DoCreateChannel,
               void(mojo::PendingReceiver<cros::mojom::CameraModule>
                        camera_module_receiver,
                    cros::mojom::CameraClientType camera_client_type));

  MOCK_METHOD1(SetTracingEnabled, void(bool enabled));

  mojo::PendingRemote<cros::mojom::CameraHalServer> GetPendingRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  mojo::Receiver<cros::mojom::CameraHalServer> receiver_{this};
  DISALLOW_COPY_AND_ASSIGN(MockCameraHalServer);
};

class MockCameraHalClient : public cros::mojom::CameraHalClient {
 public:
  MockCameraHalClient() = default;

  ~MockCameraHalClient() = default;

  void SetUpChannel(
      mojo::PendingRemote<cros::mojom::CameraModule> camera_module) override {
    DoSetUpChannel(std::move(camera_module));
  }
  MOCK_METHOD1(
      DoSetUpChannel,
      void(mojo::PendingRemote<cros::mojom::CameraModule> camera_module));

  mojo::PendingRemote<cros::mojom::CameraHalClient> GetPendingRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  mojo::Receiver<cros::mojom::CameraHalClient> receiver_{this};
  DISALLOW_COPY_AND_ASSIGN(MockCameraHalClient);
};

class MockCameraActiveClientObserver : public CameraActiveClientObserver {
 public:
  void OnActiveClientChange(cros::mojom::CameraClientType type,
                            bool is_active) override {
    DoOnActiveClientChange(type, is_active);
  }
  MOCK_METHOD2(DoOnActiveClientChange,
               void(cros::mojom::CameraClientType, bool));
};

}  // namespace

class CameraHalDispatcherImplTest : public ::testing::Test {
 public:
  CameraHalDispatcherImplTest()
      : register_client_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC) {}

  ~CameraHalDispatcherImplTest() override = default;

  void SetUp() override {
    dispatcher_ = new CameraHalDispatcherImpl();
    EXPECT_TRUE(dispatcher_->StartThreads());
  }

  void TearDown() override { delete dispatcher_; }

  scoped_refptr<base::SingleThreadTaskRunner> GetProxyTaskRunner() {
    return dispatcher_->proxy_task_runner_;
  }

  void DoLoop() {
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
  }

  void QuitRunLoop() {
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  static void RegisterServer(
      CameraHalDispatcherImpl* dispatcher,
      mojo::PendingRemote<cros::mojom::CameraHalServer> server,
      cros::mojom::CameraHalDispatcher::RegisterServerWithTokenCallback
          callback) {
    auto token = base::UnguessableToken::Create();
    dispatcher->GetTokenManagerForTesting()->AssignServerTokenForTesting(token);
    dispatcher->RegisterServerWithToken(std::move(server), std::move(token),
                                        std::move(callback));
  }

  static void RegisterClientWithToken(
      CameraHalDispatcherImpl* dispatcher,
      mojo::PendingRemote<cros::mojom::CameraHalClient> client,
      cros::mojom::CameraClientType type,
      const base::UnguessableToken& token,
      cros::mojom::CameraHalDispatcher::RegisterClientWithTokenCallback
          callback) {
    dispatcher->RegisterClientWithToken(std::move(client), type, token,
                                        std::move(callback));
  }

  void OnRegisteredServer(
      int32_t result,
      mojo::PendingRemote<cros::mojom::CameraHalServerCallbacks> callbacks) {
    if (result != 0) {
      ADD_FAILURE() << "Failed to register server: "
                    << base::safe_strerror(-result);
      QuitRunLoop();
    }
  }

  void OnRegisteredClient(int32_t result) {
    last_register_client_result_ = result;
    if (result != 0) {
      // If registration fails, CameraHalClient::SetUpChannel() will not be
      // called, and we need to quit the run loop here.
      QuitRunLoop();
    }
    register_client_event_.Signal();
  }

 protected:
  // We can't use std::unique_ptr here because the constructor and destructor of
  // CameraHalDispatcherImpl are private.
  CameraHalDispatcherImpl* dispatcher_;
  base::WaitableEvent register_client_event_;
  int32_t last_register_client_result_;

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<base::RunLoop> run_loop_;
  DISALLOW_COPY_AND_ASSIGN(CameraHalDispatcherImplTest);
};

// Test that the CameraHalDisptcherImpl correctly re-establishes a Mojo channel
// for the client when the server crashes.
TEST_F(CameraHalDispatcherImplTest, ServerConnectionError) {
  // First verify that a the CameraHalDispatcherImpl establishes a Mojo channel
  // between the server and the client.
  auto mock_server = std::make_unique<MockCameraHalServer>();
  auto mock_client = std::make_unique<MockCameraHalClient>();

  EXPECT_CALL(*mock_server, DoCreateChannel(_, _)).Times(1);
  EXPECT_CALL(*mock_client, DoSetUpChannel(_))
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));

  auto server = mock_server->GetPendingRemote();
  GetProxyTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CameraHalDispatcherImplTest::RegisterServer,
          base::Unretained(dispatcher_), std::move(server),
          base::BindOnce(&CameraHalDispatcherImplTest::OnRegisteredServer,
                         base::Unretained(this))));
  auto client = mock_client->GetPendingRemote();
  auto type = cros::mojom::CameraClientType::TESTING;
  GetProxyTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CameraHalDispatcherImplTest::RegisterClientWithToken,
          base::Unretained(dispatcher_), std::move(client), type,
          dispatcher_->GetTokenForTrustedClient(type),
          base::BindOnce(&CameraHalDispatcherImplTest::OnRegisteredClient,
                         base::Unretained(this))));

  // Wait until the client gets the established Mojo channel.
  DoLoop();

  // The client registration callback may be called after
  // CameraHalClient::SetUpChannel(). Use a waitable event to make sure we have
  // the result.
  register_client_event_.Wait();
  ASSERT_EQ(last_register_client_result_, 0);

  // Re-create a new server to simulate a server crash.
  mock_server = std::make_unique<MockCameraHalServer>();

  // Make sure we creates a new Mojo channel from the new server to the same
  // client.
  EXPECT_CALL(*mock_server, DoCreateChannel(_, _)).Times(1);
  EXPECT_CALL(*mock_client, DoSetUpChannel(_))
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));

  server = mock_server->GetPendingRemote();
  GetProxyTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CameraHalDispatcherImplTest::RegisterServer,
          base::Unretained(dispatcher_), std::move(server),
          base::BindOnce(&CameraHalDispatcherImplTest::OnRegisteredServer,
                         base::Unretained(this))));

  // Wait until the clients gets the newly established Mojo channel.
  DoLoop();
}

// Test that the CameraHalDisptcherImpl correctly re-establishes a Mojo channel
// for the client when the client reconnects after crash.
TEST_F(CameraHalDispatcherImplTest, ClientConnectionError) {
  // First verify that a the CameraHalDispatcherImpl establishes a Mojo channel
  // between the server and the client.
  auto mock_server = std::make_unique<MockCameraHalServer>();
  auto mock_client = std::make_unique<MockCameraHalClient>();

  EXPECT_CALL(*mock_server, DoCreateChannel(_, _)).Times(1);
  EXPECT_CALL(*mock_client, DoSetUpChannel(_))
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));

  auto server = mock_server->GetPendingRemote();
  GetProxyTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CameraHalDispatcherImplTest::RegisterServer,
          base::Unretained(dispatcher_), std::move(server),
          base::BindOnce(&CameraHalDispatcherImplTest::OnRegisteredServer,
                         base::Unretained(this))));
  auto client = mock_client->GetPendingRemote();
  auto type = cros::mojom::CameraClientType::TESTING;
  GetProxyTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CameraHalDispatcherImplTest::RegisterClientWithToken,
          base::Unretained(dispatcher_), std::move(client), type,
          dispatcher_->GetTokenForTrustedClient(type),
          base::BindOnce(&CameraHalDispatcherImplTest::OnRegisteredClient,
                         base::Unretained(this))));

  // Wait until the client gets the established Mojo channel.
  DoLoop();

  // The client registration callback may be called after
  // CameraHalClient::SetUpChannel(). Use a waitable event to make sure we have
  // the result.
  register_client_event_.Wait();
  ASSERT_EQ(last_register_client_result_, 0);

  // Re-create a new client to simulate a client crash.
  mock_client = std::make_unique<MockCameraHalClient>();

  // Make sure we re-create the Mojo channel from the same server to the new
  // client.
  EXPECT_CALL(*mock_server, DoCreateChannel(_, _)).Times(1);
  EXPECT_CALL(*mock_client, DoSetUpChannel(_))
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));

  client = mock_client->GetPendingRemote();
  type = cros::mojom::CameraClientType::TESTING;
  GetProxyTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CameraHalDispatcherImplTest::RegisterClientWithToken,
          base::Unretained(dispatcher_), std::move(client), type,
          dispatcher_->GetTokenForTrustedClient(type),
          base::BindOnce(&CameraHalDispatcherImplTest::OnRegisteredClient,
                         base::Unretained(this))));

  // Wait until the clients gets the newly established Mojo channel.
  DoLoop();

  // Make sure the client is still successfully registered.
  register_client_event_.Wait();
  ASSERT_EQ(last_register_client_result_, 0);
}

// Test that trusted camera HAL clients (e.g., Chrome, Android, Testing) can be
// registered successfully.
TEST_F(CameraHalDispatcherImplTest, RegisterClientSuccess) {
  // First verify that a the CameraHalDispatcherImpl establishes a Mojo channel
  // between the server and the client.
  auto mock_server = std::make_unique<MockCameraHalServer>();

  auto server = mock_server->GetPendingRemote();
  GetProxyTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CameraHalDispatcherImplTest::RegisterServer,
          base::Unretained(dispatcher_), std::move(server),
          base::BindOnce(&CameraHalDispatcherImplTest::OnRegisteredServer,
                         base::Unretained(this))));

  for (auto type : TokenManager::kTrustedClientTypes) {
    auto mock_client = std::make_unique<MockCameraHalClient>();
    EXPECT_CALL(*mock_server, DoCreateChannel(_, _)).Times(1);
    EXPECT_CALL(*mock_client, DoSetUpChannel(_))
        .Times(1)
        .WillOnce(
            InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));

    auto client = mock_client->GetPendingRemote();
    GetProxyTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &CameraHalDispatcherImplTest::RegisterClientWithToken,
            base::Unretained(dispatcher_), std::move(client), type,
            dispatcher_->GetTokenForTrustedClient(type),
            base::BindOnce(&CameraHalDispatcherImplTest::OnRegisteredClient,
                           base::Unretained(this))));

    // Wait until the client gets the established Mojo channel.
    DoLoop();

    // The client registration callback may be called after
    // CameraHalClient::SetUpChannel(). Use a waitable event to make sure we
    // have the result.
    register_client_event_.Wait();
    ASSERT_EQ(last_register_client_result_, 0);
  }
}

// Test that CameraHalClient registration fails when a wrong (empty) token is
// provided.
TEST_F(CameraHalDispatcherImplTest, RegisterClientFail) {
  // First verify that a the CameraHalDispatcherImpl establishes a Mojo channel
  // between the server and the client.
  auto mock_server = std::make_unique<MockCameraHalServer>();
  auto mock_client = std::make_unique<MockCameraHalClient>();

  auto server = mock_server->GetPendingRemote();
  GetProxyTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CameraHalDispatcherImplTest::RegisterServer,
          base::Unretained(dispatcher_), std::move(server),
          base::BindOnce(&CameraHalDispatcherImplTest::OnRegisteredServer,
                         base::Unretained(this))));

  // Use an empty token to make sure authentication fails.
  base::UnguessableToken empty_token;
  auto client = mock_client->GetPendingRemote();
  GetProxyTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CameraHalDispatcherImplTest::RegisterClientWithToken,
          base::Unretained(dispatcher_), std::move(client),
          cros::mojom::CameraClientType::TESTING, empty_token,
          base::BindOnce(&CameraHalDispatcherImplTest::OnRegisteredClient,
                         base::Unretained(this))));

  // We do not need to enter a run loop here because
  // CameraHalClient::SetUpChannel() isn't expected to called, and we only need
  // to wait for the callback from CameraHalDispatcher::RegisterClientWithToken.
  register_client_event_.Wait();
  ASSERT_EQ(last_register_client_result_, -EPERM);
}

// Test that CameraHalDispatcherImpl correctly fires CameraActiveClientObserver
// when a camera device is opened or closed by a client.
TEST_F(CameraHalDispatcherImplTest, CameraActiveClientObserverTest) {
  MockCameraActiveClientObserver observer;
  dispatcher_->AddActiveClientObserver(&observer);

  EXPECT_CALL(observer, DoOnActiveClientChange(
                            cros::mojom::CameraClientType::TESTING, true))
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));
  dispatcher_->CameraDeviceActivityChange(
      /*camera_id=*/0, /*opened=*/true, cros::mojom::CameraClientType::TESTING);

  DoLoop();

  EXPECT_CALL(observer, DoOnActiveClientChange(
                            cros::mojom::CameraClientType::TESTING, false))
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));
  dispatcher_->CameraDeviceActivityChange(
      /*camera_id=*/0, /*opened=*/false,
      cros::mojom::CameraClientType::TESTING);

  DoLoop();
}

}  // namespace media
