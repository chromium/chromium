// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
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

  void CreateChannel(mojo::PendingReceiver<cros::mojom::CameraModule>
                         camera_module_receiver) override {
    DoCreateChannel(std::move(camera_module_receiver));
  }
  MOCK_METHOD1(DoCreateChannel,
               void(mojo::PendingReceiver<cros::mojom::CameraModule>
                        camera_module_receiver));

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

}  // namespace

class CameraHalDispatcherImplTest : public ::testing::Test {
 public:
  CameraHalDispatcherImplTest() = default;

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
      mojo::PendingRemote<cros::mojom::CameraHalServer> server) {
    dispatcher->RegisterServer(std::move(server));
  }

  static void RegisterClient(
      CameraHalDispatcherImpl* dispatcher,
      mojo::PendingRemote<cros::mojom::CameraHalClient> client) {
    dispatcher->RegisterClient(std::move(client));
  }

 protected:
  // We can't use std::unique_ptr here because the constructor and destructor of
  // CameraHalDispatcherImpl are private.
  CameraHalDispatcherImpl* dispatcher_;

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

  EXPECT_CALL(*mock_server, DoCreateChannel(_)).Times(1);
  EXPECT_CALL(*mock_client, DoSetUpChannel(_))
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));

  auto server = mock_server->GetPendingRemote();
  GetProxyTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraHalDispatcherImplTest::RegisterServer,
                     base::Unretained(dispatcher_), std::move(server)));
  auto client = mock_client->GetPendingRemote();
  GetProxyTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraHalDispatcherImplTest::RegisterClient,
                     base::Unretained(dispatcher_), std::move(client)));

  // Wait until the client gets the established Mojo channel.
  DoLoop();

  // Re-create a new server to simulate a server crash.
  mock_server = std::make_unique<MockCameraHalServer>();

  // Make sure we creates a new Mojo channel from the new server to the same
  // client.
  EXPECT_CALL(*mock_server, DoCreateChannel(_)).Times(1);
  EXPECT_CALL(*mock_client, DoSetUpChannel(_))
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));

  server = mock_server->GetPendingRemote();
  GetProxyTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraHalDispatcherImplTest::RegisterServer,
                     base::Unretained(dispatcher_), std::move(server)));

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

  EXPECT_CALL(*mock_server, DoCreateChannel(_)).Times(1);
  EXPECT_CALL(*mock_client, DoSetUpChannel(_))
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));

  auto server = mock_server->GetPendingRemote();
  GetProxyTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraHalDispatcherImplTest::RegisterServer,
                     base::Unretained(dispatcher_), std::move(server)));
  auto client = mock_client->GetPendingRemote();
  GetProxyTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraHalDispatcherImplTest::RegisterClient,
                     base::Unretained(dispatcher_), std::move(client)));

  // Wait until the client gets the established Mojo channel.
  DoLoop();

  // Re-create a new server to simulate a server crash.
  mock_client = std::make_unique<MockCameraHalClient>();

  // Make sure we re-create the Mojo channel from the same server to the new
  // client.
  EXPECT_CALL(*mock_server, DoCreateChannel(_)).Times(1);
  EXPECT_CALL(*mock_client, DoSetUpChannel(_))
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));

  client = mock_client->GetPendingRemote();
  GetProxyTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraHalDispatcherImplTest::RegisterClient,
                     base::Unretained(dispatcher_), std::move(client)));

  // Wait until the clients gets the newly established Mojo channel.
  DoLoop();
}

}  // namespace media
