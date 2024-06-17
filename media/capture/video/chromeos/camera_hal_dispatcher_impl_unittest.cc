// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
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

class MockCrosCameraService : public cros::mojom::CrosCameraService {
 public:
  MockCrosCameraService() = default;

  MockCrosCameraService(const MockCrosCameraService&) = delete;
  MockCrosCameraService& operator=(const MockCrosCameraService&) = delete;

  ~MockCrosCameraService() override = default;

  // **NOTE**: If you add additional mocks here, you will need to
  //           carefully add an EXPECT_CALL with a WillOnce to invoke
  //           CameraHalDispatcherImplTest::QuitRunLoop and increment
  //           RunLoop(val) appropriately. Failing to do this will
  //           introduce flakiness into these tests.
  MOCK_METHOD2(GetCameraModule,
               void(cros::mojom::CameraClientType camera_client_type,
                    GetCameraModuleCallback callback));

  MOCK_METHOD1(SetTracingEnabled, void(bool enabled));
  MOCK_METHOD1(SetAutoFramingState,
               void(cros::mojom::CameraAutoFramingState state));
  MOCK_METHOD1(
      GetCameraSWPrivacySwitchState,
      void(cros::mojom::CrosCameraService::GetCameraSWPrivacySwitchStateCallback
               callback));
  MOCK_METHOD1(SetCameraSWPrivacySwitchState,
               void(cros::mojom::CameraPrivacySwitchState state));
  MOCK_METHOD1(GetAutoFramingSupported,
               void(GetAutoFramingSupportedCallback callback));
  MOCK_METHOD2(SetCameraEffect,
               void(::cros::mojom::EffectsConfigPtr config,
                    SetCameraEffectCallback callback));
  MOCK_METHOD1(AddCrosCameraServiceObserver,
               void(mojo::PendingRemote<cros::mojom::CrosCameraServiceObserver>
                        observer));

  // **NOTE**: Please read the note at the top of these mocks if you're
  //           adding more mocks.

  mojo::PendingRemote<cros::mojom::CrosCameraService> GetPendingRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  // `cros::mojom::CrosCameraService` implementation of non relevant methods.
  void StartKioskVisionDetection(
      const std::string& dlc_path,
      mojo::PendingRemote<cros::mojom::KioskVisionObserver> observer) override {
  }

  mojo::Receiver<cros::mojom::CrosCameraService> receiver_{this};
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
  // Observers are notified when dispatcher_->SetCameraEffects is complete.
  // The `new_effects` is saved internally, and compared later on with expected
  // effects.
  void OnCameraEffectChanged(
      const cros::mojom::EffectsConfigPtr& new_effects) override {
    new_effects_ = new_effects.Clone();
    DoOnCameraEffectChanged();
  }

  MOCK_METHOD0(DoOnCameraEffectChanged, void());

  const cros::mojom::EffectsConfigPtr& new_effects() { return new_effects_; }

 private:
  cros::mojom::EffectsConfigPtr new_effects_;
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
    dispatcher_->initial_effects_ = GetDefaultCameraEffectsConfigForTesting();
  }

  void TearDown() override { delete dispatcher_.ExtractAsDangling(); }

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

  static void BindCameraService(
      CameraHalDispatcherImpl* dispatcher,
      mojo::PendingRemote<cros::mojom::CrosCameraService> camera_service) {
    dispatcher->BindCameraServiceOnProxyThread(std::move(camera_service));
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
  void SetCameraEffectsWithDispatcher(cros::mojom::EffectsConfigPtr config) {
    dispatcher_->SetCameraEffects(std::move(config));
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
  raw_ptr<CameraHalDispatcherImpl> dispatcher_;
  base::WaitableEvent register_client_event_;
  int32_t last_register_client_result_;
  std::atomic<int> quit_count_ = 0;

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

// Test that the CameraHalDisptcherImpl correctly re-establishes a Mojo channel
// for the client when the client reconnects after crash.
TEST_F(CameraHalDispatcherImplTest, ClientConnectionError) {
  // First verify that a the CameraHalDispatcherImpl establishes a Mojo channel
  // between the camera service and the client.
  auto mock_service = std::make_unique<MockCrosCameraService>();
  auto mock_client = std::make_unique<MockCameraHalClient>();

  CreateLoop(5);
  EXPECT_CALL(*mock_service, GetCameraModule(_, _))
      .Times(1)
      .WillOnce(
          [this](cros::mojom::CameraClientType camera_client_type,
                 MockCrosCameraService::GetCameraModuleCallback callback) {
            mojo::PendingReceiver<cros::mojom::CameraModule> receiver;
            std::move(callback).Run(receiver.InitWithNewPipeAndPassRemote());
            this->QuitRunLoop();
          });
  EXPECT_CALL(*mock_client, DoSetUpChannel(_))
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));
  EXPECT_CALL(*mock_service, SetAutoFramingState(_))
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));
  EXPECT_CALL(*mock_service, SetCameraEffect(_, _))
      .Times(1)
      .WillOnce(
          [this](::cros::mojom::EffectsConfigPtr,
                 MockCrosCameraService::SetCameraEffectCallback callback) {
            std::move(callback).Run(::cros::mojom::SetEffectResult::kOk);
            this->QuitRunLoop();
          });
  EXPECT_CALL(*mock_service, AddCrosCameraServiceObserver(_))
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));

  auto service = mock_service->GetPendingRemote();
  GetProxyTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraHalDispatcherImplTest::BindCameraService,
                     base::Unretained(dispatcher_), std::move(service)));
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

  // Make sure we re-create the Mojo channel from the same camera service
  // instance to the new client.
  EXPECT_CALL(*mock_service, GetCameraModule(_, _))
      .Times(1)
      .WillOnce(
          [this](cros::mojom::CameraClientType camera_client_type,
                 MockCrosCameraService::GetCameraModuleCallback callback) {
            mojo::PendingReceiver<cros::mojom::CameraModule> receiver;
            std::move(callback).Run(receiver.InitWithNewPipeAndPassRemote());
            this->QuitRunLoop();
          });
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
  // between the camera service and the client.
  auto mock_service = std::make_unique<MockCrosCameraService>();

  auto service = mock_service->GetPendingRemote();
  GetProxyTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraHalDispatcherImplTest::BindCameraService,
                     base::Unretained(dispatcher_), std::move(service)));

  bool firstRun = true;
  for (auto type : TokenManager::kTrustedClientTypes) {
    int loopsRequired = 2;
    auto mock_client = std::make_unique<MockCameraHalClient>();
    EXPECT_CALL(*mock_service, GetCameraModule(_, _))
        .Times(1)
        .WillOnce(
            [this](cros::mojom::CameraClientType camera_client_type,
                   MockCrosCameraService::GetCameraModuleCallback callback) {
              mojo::PendingReceiver<cros::mojom::CameraModule> receiver;
              std::move(callback).Run(receiver.InitWithNewPipeAndPassRemote());
              this->QuitRunLoop();
            });
    EXPECT_CALL(*mock_client, DoSetUpChannel(_))
        .Times(1)
        .WillOnce(
            InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));
    if (firstRun) {
      EXPECT_CALL(*mock_service, SetAutoFramingState(_))
          .Times(1)
          .WillOnce(InvokeWithoutArgs(
              this, &CameraHalDispatcherImplTest::QuitRunLoop));
      EXPECT_CALL(*mock_service, SetCameraEffect(_, _))
          .Times(1)
          .WillOnce(
              [this](::cros::mojom::EffectsConfigPtr,
                     MockCrosCameraService::SetCameraEffectCallback callback) {
                std::move(callback).Run(::cros::mojom::SetEffectResult::kOk);
                this->QuitRunLoop();
              });
      EXPECT_CALL(*mock_service, AddCrosCameraServiceObserver(_))
          .Times(1)
          .WillOnce(InvokeWithoutArgs(
              this, &CameraHalDispatcherImplTest::QuitRunLoop));
      // These above calls only happen on the first client connection
      firstRun = false;
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
  // between the camera service and the client.
  auto mock_service = std::make_unique<MockCrosCameraService>();
  auto mock_client = std::make_unique<MockCameraHalClient>();

  // Extra RunLoop for failure to register the client. In |OnRegisteredClient|,
  // |QuitRunLoop| is called if the registration fails.
  CreateLoop(4);
  EXPECT_CALL(*mock_service, SetAutoFramingState(_))
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));
  EXPECT_CALL(*mock_service, SetCameraEffect(_, _))
      .Times(1)
      .WillOnce(
          [this](::cros::mojom::EffectsConfigPtr,
                 MockCrosCameraService::SetCameraEffectCallback callback) {
            std::move(callback).Run(::cros::mojom::SetEffectResult::kOk);
            this->QuitRunLoop();
          });
  EXPECT_CALL(*mock_service, AddCrosCameraServiceObserver(_))
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));

  auto service = mock_service->GetPendingRemote();
  GetProxyTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraHalDispatcherImplTest::BindCameraService,
                     base::Unretained(dispatcher_), std::move(service)));

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
  dispatcher_->AddCameraEffectObserver(&observer);
  cros::mojom::EffectsConfigPtr config =
      GetDefaultCameraEffectsConfigForTesting();
  // Set effects for the first time.
  CreateLoop(1);
  EXPECT_CALL(observer, DoOnCameraEffectChanged())
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));
  GetProxyTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraHalDispatcherImplTest::SetCameraEffectsComplete,
                     base::Unretained(dispatcher_), config.Clone(),
                     /*is_from_register=*/true,
                     cros::mojom::SetEffectResult::kOk));
  RunLoop();
  EXPECT_EQ(observer.new_effects(), config);

  cros::mojom::EffectsConfigPtr new_config =
      GetDefaultCameraEffectsConfigForTesting();
  new_config->blur_enabled = !new_config->blur_enabled;
  new_config->relight_enabled = !new_config->relight_enabled;
  new_config->replace_enabled = !new_config->replace_enabled;

  // Fire default config if the setting is from register and failed.
  CreateLoop(1);
  EXPECT_CALL(observer, DoOnCameraEffectChanged())
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));
  GetProxyTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraHalDispatcherImplTest::SetCameraEffectsComplete,
                     base::Unretained(dispatcher_), new_config.Clone(),
                     /*is_from_register=*/true,
                     cros::mojom::SetEffectResult::kError));
  RunLoop();
  EXPECT_EQ(observer.new_effects(), cros::mojom::EffectsConfig::New());

  // Fire previous config if the setting is not from register and failed.
  CreateLoop(1);
  EXPECT_CALL(observer, DoOnCameraEffectChanged())
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));
  GetProxyTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraHalDispatcherImplTest::SetCameraEffectsComplete,
                     base::Unretained(dispatcher_), new_config.Clone(),
                     /*is_from_register=*/false,
                     cros::mojom::SetEffectResult::kError));
  RunLoop();
  EXPECT_EQ(observer.new_effects(), cros::mojom::EffectsConfig::New());

  // Fire new config is the setting is successful.
  CreateLoop(1);
  EXPECT_CALL(observer, DoOnCameraEffectChanged())
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));
  GetProxyTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraHalDispatcherImplTest::SetCameraEffectsComplete,
                     base::Unretained(dispatcher_), new_config.Clone(),
                     /*is_from_register=*/false,
                     cros::mojom::SetEffectResult::kOk));
  RunLoop();
  EXPECT_EQ(observer.new_effects(), new_config);
}

// Test that SetCameraEffects behave correctly.
TEST_F(CameraHalDispatcherImplTest, SetCameraEffects) {
  MockCameraEffectObserver observer;
  dispatcher_->AddCameraEffectObserver(&observer);
  // Case (1) SetCameraEffects should fail if the camera service is not
  // initialized.
  CreateLoop(1);
  cros::mojom::EffectsConfigPtr config = cros::mojom::EffectsConfig::New();
  EXPECT_CALL(observer, DoOnCameraEffectChanged())
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));
  SetCameraEffectsWithDispatcher(config.Clone());
  RunLoop();
  EXPECT_EQ(observer.new_effects(), cros::mojom::EffectsConfigPtr());

  auto mock_service = std::make_unique<MockCrosCameraService>();

  // Case (2) Connect mock_serice will trigger a mock_serice->SetCameraEffect
  // call, and we want let this one to succeed.
  CreateLoop(4);
  EXPECT_CALL(*mock_service, SetAutoFramingState(_))
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));
  EXPECT_CALL(*mock_service, SetCameraEffect(_, _))
      .Times(1)
      .WillOnce(
          [this](::cros::mojom::EffectsConfigPtr,
                 MockCrosCameraService::SetCameraEffectCallback callback) {
            std::move(callback).Run(::cros::mojom::SetEffectResult::kOk);
            this->QuitRunLoop();
          });
  EXPECT_CALL(*mock_service, AddCrosCameraServiceObserver(_))
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));
  EXPECT_CALL(observer, DoOnCameraEffectChanged())
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));

  auto service = mock_service->GetPendingRemote();
  GetProxyTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraHalDispatcherImplTest::BindCameraService,
                     base::Unretained(dispatcher_), std::move(service)));
  RunLoop();
  EXPECT_EQ(observer.new_effects(), config);

  // Case (3) if mock_server->SetCameraEffect succeeds, the expected camera
  // effects should be updated.
  CreateLoop(2);
  EXPECT_CALL(*mock_service, SetCameraEffect(_, _))
      .Times(1)
      .WillOnce(
          [this](::cros::mojom::EffectsConfigPtr config,
                 MockCrosCameraService::SetCameraEffectCallback callback) {
            std::move(callback).Run(::cros::mojom::SetEffectResult::kOk);
            this->QuitRunLoop();
          });
  EXPECT_CALL(observer, DoOnCameraEffectChanged())
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));

  config->blur_enabled = true;
  SetCameraEffectsWithDispatcher(config.Clone());
  RunLoop();
  EXPECT_EQ(observer.new_effects(), config);

  // Case (4) if mock_service->SetCameraEffect fails, the expected camera
  // effects should not be updated.
  CreateLoop(2);
  EXPECT_CALL(*mock_service, SetCameraEffect(_, _))
      .Times(1)
      .WillOnce(
          [this](::cros::mojom::EffectsConfigPtr config,
                 MockCrosCameraService::SetCameraEffectCallback callback) {
            std::move(callback).Run(::cros::mojom::SetEffectResult::kError);
            this->QuitRunLoop();
          });

  EXPECT_CALL(observer, DoOnCameraEffectChanged())
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &CameraHalDispatcherImplTest::QuitRunLoop));

  SetCameraEffectsWithDispatcher(GetDefaultCameraEffectsConfigForTesting());
  RunLoop();
  EXPECT_EQ(observer.new_effects(), config);
}

}  // namespace media
