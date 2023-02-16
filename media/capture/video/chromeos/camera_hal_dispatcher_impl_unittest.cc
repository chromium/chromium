// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/posix/safe_strerror.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "media/capture/video/chromeos/mojom/camera_common.mojom.h"
#include "media/capture/video/chromeos/mojom/cros_camera_client.mojom.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom.h"
#include "media/capture/video/chromeos/mojom/effects_pipeline.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::InvokeWithoutArgs;

namespace media {
namespace {

// Returns a EffectsConfigPtr for the testing purpose only.
cros::mojom::EffectsConfigPtr GetDefaultCameraEffectsConfigForTesting() {
  cros::mojom::EffectsConfigPtr config = cros::mojom::EffectsConfig::New();
  config->blur_enabled = false;
  config->replace_enabled = true;
  config->relight_enabled = true;
  return config;
}

class MockCameraHalServer : public cros::mojom::CameraHalServer {
 public:
  MockCameraHalServer() = default;

  MockCameraHalServer(const MockCameraHalServer&) = delete;
  MockCameraHalServer& operator=(const MockCameraHalServer&) = delete;

  ~MockCameraHalServer() override = default;

  void CreateChannel(
      mojo::PendingReceiver<cros::mojom::CameraModule> camera_module_receiver,
      cros::mojom::CameraClientType camera_client_type) override {
    DoCreateChannel(std::move(camera_module_receiver), camera_client_type);
  }

  // **NOTE**: If you add additional mocks here, you will need to
  //           carefully add an EXPECT_CALL with a WillOnce to invoke
  //           CameraHalDispatcherImplTest::QuitRunLoop and increment
  //           RunLoop(val) appropriately. Failing to do this will
  //           introduce flakiness into these tests.
  MOCK_METHOD2(DoCreateChannel,
               void(mojo::PendingReceiver<cros::mojom::CameraModule>
                        camera_module_receiver,
                    cros::mojom::CameraClientType camera_client_type));

  MOCK_METHOD1(SetTracingEnabled, void(bool enabled));
  MOCK_METHOD1(SetAutoFramingState,
               void(cros::mojom::CameraAutoFramingState state));
  MOCK_METHOD1(
      GetCameraSWPrivacySwitchState,
      void(cros::mojom::CameraHalServer::GetCameraSWPrivacySwitchStateCallback
               callback));
  MOCK_METHOD1(SetCameraSWPrivacySwitchState,
               void(cros::mojom::CameraPrivacySwitchState state));
  MOCK_METHOD1(GetAutoFramingSupported,
               void(GetAutoFramingSupportedCallback callback));
  MOCK_METHOD2(SetCameraEffect,
               void(::cros::mojom::EffectsConfigPtr config,
                    SetCameraEffectCallback callback));
  // **NOTE**: Please read the note at the top of these mocks if you're
  //           adding more mocks.

  mojo::PendingRemote<cros::mojom::CameraHalServer> GetPendingRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  mojo::Receiver<cros::mojom::CameraHalServer> receiver_{this};
};

class MockCameraHalClient : public cros::mojom::CameraHalClient {
 public:
  MockCameraHalClient() = default;

  MockCameraHalClient(const MockCameraHalClient&) = delete;
  MockCameraHalClient& operator=(const MockCameraHalClient&) = delete;

  ~MockCameraHalClient() override = default;

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
};

class MockCameraActiveClientObserver : public CameraActiveClientObserver {
 public:
  void OnActiveClientChange(
      cros::mojom::CameraClientType type,
      bool is_active,
      const base::flat_set<std::string>& device_ids) override {
    DoOnActiveClientChange(type, is_active, device_ids);
  }
  MOCK_METHOD3(DoOnActiveClientChange,
               void(cros::mojom::CameraClientType,
                    bool,
                    const base::flat_set<std::string>&));
};

// This observer needs to be mocked to observe the completion of
// dispatcher_->SetCameraEffects.
class MockCameraEffectObserver : public CameraEffectObserver {
 public:
  MockCameraEffectObserver()
      : expected_camera_effects_config_(
            GetDefaultCameraEffectsConfigForTesting()) {}

  // Observers are notified when dispatcher_->SetCameraEffects is complete.
  // A caller should first set `expected_camera_effects_config_`, this function
  // will then compare that the notification is indeed sending these expected
  // values.
  void OnCameraEffectChanged(
      const cros::mojom::EffectsConfigPtr& new_effects) override {
    EXPECT_EQ(expected_camera_effects_config_, new_effects);
    DoOnCameraEffectChanged();
  }
  MOCK_METHOD0(DoOnCameraEffectChanged, void());

  cros::mojom::EffectsConfigPtr expected_camera_effects_config_;
};

}  // namespace

class CameraHalDispatcherImplTest : public ::testing::Test {
 public:
  CameraHalDispatcherImplTest()
      : register_client_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC) {}

  CameraHalDispatcherImplTest(const CameraHalDispatcherImplTest&) = delete;
  CameraHalDispatcherImplTest& operator=(const CameraHalDispatcherImplTest&) =
      delete;

  ~CameraHalDispatcherImplTest() override = default;

  void SetUp() override {
    dispatcher_ = new CameraHalDispatcherImpl();
    dispatcher_->AddCameraIdToDeviceIdEntry(0, "0");

    EXPECT_TRUE(dispatcher_->StartThreads());

    // Initialize camera effects parameters. These require threads
    // to be running.
    dispatcher_->SetInitialCameraEffects(
        GetDefaultCameraEffectsConfigForTesting());
    dispatcher_->SetCameraEffectsControllerCallback(base::BindRepeating(
        &CameraHalDispatcherImplTest::SetCameraEffectsCallback,
        base::Unretained(this)));
  }

  void TearDown() override { delete dispatcher_; }

  scoped_refptr<base::SingleThreadTaskRunner> GetProxyTaskRunner() {
    return dispatcher_->proxy_task_runner_;
  }

  void CreateLoop(int expected_quit_count) {
    quit_count_ = expected_quit_count;
    run_loop_ = std::make_unique<base::RunLoop>();
  }

  void RunLoop() { run_loop_->Run(); }

  void QuitRunLoop() {
    if (run_loop_) {
      quit_count_--;
      if (quit_count_ == 0) {
        run_loop_->Quit();
      }
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

  // Sets camera effects with dispatcher_.
  // This helper function is needed because SetCameraEffects is a private
  // function.
  void SetCameraEffectsWithExpect(cros::mojom::EffectsConfigPtr config) {
    dispatcher_->SetCameraEffects(std::move(config));
  }

  // Callback that is called when dispatcher_->SetCameraEffects is complete.
  // A caller should first set `expected_camera_effects_result_` and
  // `expected_camera_effects_config_`, this function will then compare that
  // the callback is indeed called on these expected values.
  // This function also has QuitRunLoop so that we can wait until it finishes.
  void SetCameraEffectsCallback(cros::mojom::EffectsConfigPtr config,
                                cros::mojom::SetEffectResult result) {
    EXPECT_EQ(expected_camera_effects_result_, result);
    EXPECT_TRUE(
        expected_camera_effects_config_.Equals(dispatcher_->current_effects_));
    QuitRunLoop();
  }

  // Call OnSetCameraEffectsCompleteOnProxyThread with dispatcher_.
  // This helper function is needed because
  // OnSetCameraEffectsCompleteOnProxyThread is a private function.
  static void SetCameraEffectsComplete(CameraHalDispatcherImpl* dispatcher,
                                       cros::mojom::EffectsConfigPtr config,
                                       bool is_from_register,
                                       cros::mojom::SetEffectResult result) {
    dispatcher->OnSetCameraEffectsCompleteOnProxyThread(
        std::move(config), is_from_register, result);
  }

 protected:
  // We can't use std::unique_ptr here because the constructor and destructor of
  // CameraHalDispatcherImpl are private.
  CameraHalDispatcherImpl* dispatcher_;
  base::WaitableEvent register_client_event_;
  int32_t last_register_client_result_;
  std::atomic<int> quit_count_ = 0;

  cros::mojom::SetEffectResult expected_camera_effects_result_ =
      cros::mojom::SetEffectResult::kOk;
  cros::mojom::EffectsConfigPtr expected_camera_effects_config_ =
      GetDefaultCameraEffectsConfigForTesting();

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

// Test that the CameraHalDisptcherImpl correctly re-establishes a Mojo channel
// for the client when the server crashes.
TEST_F(CameraHalDispatcherImplTest, ServerConnectionError) {
  // First verify that a the CameraHalDispatcherImpl establishes a Mojo channel
  // between the server and the client.
  auto mock_server = std::make_unique<MockCameraHalServer>();
  auto mock_client = std::make_unique<MockCameraHalClient>();

  // Wait until the client gets the established Mojo channel, and that
  // all expected mojo calls have been invoked.
  CreateLoop(6);
  MockCameraActiveClientObserver observer;
  dispatcher_->AddActiveClientObserver(&observer);
  dispatcher_->CameraDeviceActivityChange(
      /*camera_id=*/0, /*opened=*/true, cros::mojom::CameraClientType::TESTING);

  EXPECT_CALL(*mock_server, DoCreateChannel(_, _))
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));
  EXPECT_CALL(*mock_client, DoSetUpChannel(_))
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));
  EXPECT_CALL(*mock_server, SetAutoFramingState(_))
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));
  EXPECT_CALL(*mock_server, SetCameraEffect(_, _))
      .Times(1)
      .WillOnce([this](::cros::mojom::EffectsConfigPtr,
                       MockCameraHalServer::SetCameraEffectCallback callback) {
        std::move(callback).Run(::cros::mojom::SetEffectResult::kOk);
        this->QuitRunLoop();
      });
  EXPECT_CALL(observer,
              DoOnActiveClientChange(cros::mojom::CameraClientType::TESTING,
                                     true, base::flat_set<std::string>({"0"})))
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
  RunLoop();

  // The client registration callback may be called after
  // CameraHalClient::SetUpChannel(). Use a waitable event to make sure we have
  // the result.
  register_client_event_.Wait();
  ASSERT_EQ(last_register_client_result_, 0);

  CreateLoop(1);
  // Re-create a new server to simulate a server crash.
  mock_server = std::make_unique<MockCameraHalServer>();

  // Wait for our observer to be told the client is inactive, which means
  // the server has been cleaned up properly before registering again.
  EXPECT_CALL(observer,
              DoOnActiveClientChange(cros::mojom::CameraClientType::TESTING,
                                     false, base::flat_set<std::string>()))
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));
  RunLoop();

  // Make sure we create a new Mojo channel from the new server to the same
  // client.
  CreateLoop(5);
  EXPECT_CALL(*mock_server, DoCreateChannel(_, _))
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));
  EXPECT_CALL(*mock_client, DoSetUpChannel(_))
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));
  EXPECT_CALL(*mock_server, SetAutoFramingState(_))
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));
  EXPECT_CALL(*mock_server, SetCameraEffect(_, _))
      .Times(1)
      .WillOnce([this](::cros::mojom::EffectsConfigPtr,
                       MockCameraHalServer::SetCameraEffectCallback callback) {
        std::move(callback).Run(::cros::mojom::SetEffectResult::kOk);
        this->QuitRunLoop();
      });

  server = mock_server->GetPendingRemote();
  GetProxyTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CameraHalDispatcherImplTest::RegisterServer,
          base::Unretained(dispatcher_), std::move(server),
          base::BindOnce(&CameraHalDispatcherImplTest::OnRegisteredServer,
                         base::Unretained(this))));
  // Wait until the client gets the established Mojo channel, and that
  // all expected mojo calls have been invoked.
  RunLoop();
}

// Test that the CameraHalDisptcherImpl correctly re-establishes a Mojo channel
// for the client when the client reconnects after crash.
TEST_F(CameraHalDispatcherImplTest, ClientConnectionError) {
  // First verify that a the CameraHalDispatcherImpl establishes a Mojo channel
  // between the server and the client.
  auto mock_server = std::make_unique<MockCameraHalServer>();
  auto mock_client = std::make_unique<MockCameraHalClient>();

  CreateLoop(5);
  EXPECT_CALL(*mock_server, DoCreateChannel(_, _))
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));
  EXPECT_CALL(*mock_client, DoSetUpChannel(_))
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));
  EXPECT_CALL(*mock_server, SetAutoFramingState(_))
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));
  EXPECT_CALL(*mock_server, SetCameraEffect(_, _))
      .Times(1)
      .WillOnce([this](::cros::mojom::EffectsConfigPtr,
                       MockCameraHalServer::SetCameraEffectCallback callback) {
        std::move(callback).Run(::cros::mojom::SetEffectResult::kOk);
        this->QuitRunLoop();
      });

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
  // Wait until the client gets the established Mojo channel, and that
  // all expected mojo calls have been invoked.
  RunLoop();

  // The client registration callback may be called after
  // CameraHalClient::SetUpChannel(). Use a waitable event to make sure we have
  // the result.
  register_client_event_.Wait();
  ASSERT_EQ(last_register_client_result_, 0);

  CreateLoop(2);
  // Re-create a new client to simulate a client crash.
  mock_client = std::make_unique<MockCameraHalClient>();

  // Make sure we re-create the Mojo channel from the same server to the new
  // client.
  EXPECT_CALL(*mock_server, DoCreateChannel(_, _))
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));
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

  // Wait until the clients gets the newly established Mojo channel, and that
  // all expected mojo calls have been invoked.
  RunLoop();

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

  bool firstRun = true;
  for (auto type : TokenManager::kTrustedClientTypes) {
    int loopsRequired = 2;
    auto mock_client = std::make_unique<MockCameraHalClient>();
    EXPECT_CALL(*mock_server, DoCreateChannel(_, _))
        .Times(1)
        .WillOnce(
            InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));
    EXPECT_CALL(*mock_client, DoSetUpChannel(_))
        .Times(1)
        .WillOnce(
            InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));
    if (firstRun) {
      EXPECT_CALL(*mock_server, SetAutoFramingState(_))
          .Times(1)
          .WillOnce(InvokeWithoutArgs(
              this, &CameraHalDispatcherImplTest::QuitRunLoop));
      EXPECT_CALL(*mock_server, SetCameraEffect(_, _))
          .Times(1)
          .WillOnce(
              [this](::cros::mojom::EffectsConfigPtr,
                     MockCameraHalServer::SetCameraEffectCallback callback) {
                std::move(callback).Run(::cros::mojom::SetEffectResult::kOk);
                this->QuitRunLoop();
              });
      // These above calls only happen on the first client connection
      firstRun = false;
      // Extra call for SetCameraEffectsCallback
      loopsRequired += 3;
    }
    CreateLoop(loopsRequired);
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
    RunLoop();

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

  // Extra RunLoop for SetCameraEffectsCallback
  CreateLoop(3);
  EXPECT_CALL(*mock_server, SetAutoFramingState(_))
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));
  EXPECT_CALL(*mock_server, SetCameraEffect(_, _))
      .Times(1)
      .WillOnce([this](::cros::mojom::EffectsConfigPtr,
                       MockCameraHalServer::SetCameraEffectCallback callback) {
        std::move(callback).Run(::cros::mojom::SetEffectResult::kOk);
        this->QuitRunLoop();
      });

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

  RunLoop();

  register_client_event_.Wait();
  ASSERT_EQ(last_register_client_result_, -EPERM);
}

// Test that CameraHalDispatcherImpl correctly fires CameraActiveClientObserver
// when a camera device is opened or closed by a client.
TEST_F(CameraHalDispatcherImplTest, CameraActiveClientObserverTest) {
  MockCameraActiveClientObserver observer;
  dispatcher_->AddActiveClientObserver(&observer);

  CreateLoop(1);
  EXPECT_CALL(observer,
              DoOnActiveClientChange(cros::mojom::CameraClientType::TESTING,
                                     true, base::flat_set<std::string>({"0"})))
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));
  dispatcher_->CameraDeviceActivityChange(
      /*camera_id=*/0, /*opened=*/true, cros::mojom::CameraClientType::TESTING);
  RunLoop();

  CreateLoop(1);
  EXPECT_CALL(observer,
              DoOnActiveClientChange(cros::mojom::CameraClientType::TESTING,
                                     false, base::flat_set<std::string>()))
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));
  dispatcher_->CameraDeviceActivityChange(
      /*camera_id=*/0, /*opened=*/false,
      cros::mojom::CameraClientType::TESTING);
  RunLoop();
}

// Test that CameraHalDispatcherImpl correctly fires CameraEffectObserver when
// the mojom call is replied from camera hal server.
TEST_F(CameraHalDispatcherImplTest, CameraEffectObserver) {
  MockCameraEffectObserver observer;
  dispatcher_->AddCameraEffectObserver(&observer, base::DoNothing());
  cros::mojom::EffectsConfigPtr config =
      GetDefaultCameraEffectsConfigForTesting();
  // Set effects for the first time.
  CreateLoop(1);
  expected_camera_effects_result_ = cros::mojom::SetEffectResult::kOk;
  expected_camera_effects_config_ = config.Clone();
  observer.expected_camera_effects_config_ = config.Clone();
  GetProxyTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraHalDispatcherImplTest::SetCameraEffectsComplete,
                     base::Unretained(dispatcher_), config.Clone(),
                     /*is_from_register=*/true,
                     cros::mojom::SetEffectResult::kOk));
  RunLoop();

  cros::mojom::EffectsConfigPtr new_config =
      GetDefaultCameraEffectsConfigForTesting();
  new_config->blur_enabled = !new_config->blur_enabled;
  new_config->relight_enabled = !new_config->relight_enabled;
  new_config->replace_enabled = !new_config->replace_enabled;

  // Fire default config if the setting is from register and failed.
  CreateLoop(2);
  EXPECT_CALL(observer, DoOnCameraEffectChanged())
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));
  expected_camera_effects_result_ = cros::mojom::SetEffectResult::kError;
  expected_camera_effects_config_ = config.Clone();
  observer.expected_camera_effects_config_ = cros::mojom::EffectsConfig::New();
  GetProxyTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraHalDispatcherImplTest::SetCameraEffectsComplete,
                     base::Unretained(dispatcher_), new_config.Clone(),
                     /*is_from_register=*/true,
                     cros::mojom::SetEffectResult::kError));
  RunLoop();

  // Fire previous config if the setting is not from register and failed.
  CreateLoop(2);
  EXPECT_CALL(observer, DoOnCameraEffectChanged())
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));
  expected_camera_effects_result_ = cros::mojom::SetEffectResult::kError;
  expected_camera_effects_config_ = config.Clone();
  observer.expected_camera_effects_config_ = config.Clone();
  GetProxyTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraHalDispatcherImplTest::SetCameraEffectsComplete,
                     base::Unretained(dispatcher_), new_config.Clone(),
                     /*is_from_register=*/false,
                     cros::mojom::SetEffectResult::kError));
  RunLoop();

  // Fire new config is the setting is successful.
  CreateLoop(2);
  EXPECT_CALL(observer, DoOnCameraEffectChanged())
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));
  expected_camera_effects_result_ = cros::mojom::SetEffectResult::kOk;
  expected_camera_effects_config_ = new_config.Clone();
  observer.expected_camera_effects_config_ = new_config.Clone();
  GetProxyTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraHalDispatcherImplTest::SetCameraEffectsComplete,
                     base::Unretained(dispatcher_), new_config.Clone(),
                     /*is_from_register=*/false,
                     cros::mojom::SetEffectResult::kOk));
  RunLoop();
}

// Test that SetCameraEffects behave correctly.
TEST_F(CameraHalDispatcherImplTest, SetCameraEffects) {
  // Case (1) SetCameraEffects should fail if server is not initialized.
  CreateLoop(1);
  cros::mojom::EffectsConfigPtr config = cros::mojom::EffectsConfig::New();
  expected_camera_effects_result_ = cros::mojom::SetEffectResult::kError;
  expected_camera_effects_config_.reset();
  SetCameraEffectsWithExpect(config.Clone());
  RunLoop();

  auto mock_server = std::make_unique<MockCameraHalServer>();

  // Case (2) Connect mock_server will trigger a mock_server->SetCameraEffect
  // call, and we want let this one to succeed.
  expected_camera_effects_result_ = cros::mojom::SetEffectResult::kOk;
  expected_camera_effects_config_ = GetDefaultCameraEffectsConfigForTesting();
  CreateLoop(3);
  EXPECT_CALL(*mock_server, SetAutoFramingState(_))
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));
  EXPECT_CALL(*mock_server, SetCameraEffect(_, _))
      .Times(1)
      .WillOnce([this](::cros::mojom::EffectsConfigPtr,
                       MockCameraHalServer::SetCameraEffectCallback callback) {
        std::move(callback).Run(::cros::mojom::SetEffectResult::kOk);
        this->QuitRunLoop();
      });

  auto server = mock_server->GetPendingRemote();
  GetProxyTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CameraHalDispatcherImplTest::RegisterServer,
          base::Unretained(dispatcher_), std::move(server),
          base::BindOnce(&CameraHalDispatcherImplTest::OnRegisteredServer,
                         base::Unretained(this))));
  RunLoop();

  // Case (3) if mock_server->SetCameraEffect succeeds, the expected camera
  // effects should be updated.
  CreateLoop(2);
  EXPECT_CALL(*mock_server, SetCameraEffect(_, _))
      .Times(1)
      .WillOnce([this](::cros::mojom::EffectsConfigPtr config,
                       MockCameraHalServer::SetCameraEffectCallback callback) {
        std::move(callback).Run(::cros::mojom::SetEffectResult::kOk);
        this->QuitRunLoop();
      });

  expected_camera_effects_result_ = cros::mojom::SetEffectResult::kOk;
  expected_camera_effects_config_ = config.Clone();
  SetCameraEffectsWithExpect(config.Clone());
  RunLoop();

  // Case (4) if mock_server->SetCameraEffect fails, the expected camera effects
  // should not be updated.
  CreateLoop(2);
  EXPECT_CALL(*mock_server, SetCameraEffect(_, _))
      .Times(1)
      .WillOnce([this](::cros::mojom::EffectsConfigPtr config,
                       MockCameraHalServer::SetCameraEffectCallback callback) {
        std::move(callback).Run(::cros::mojom::SetEffectResult::kError);
        this->QuitRunLoop();
      });

  expected_camera_effects_result_ = cros::mojom::SetEffectResult::kError;
  config = GetDefaultCameraEffectsConfigForTesting();
  SetCameraEffectsWithExpect(config.Clone());
  RunLoop();
}

}  // namespace media
