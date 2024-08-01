// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "build/chromeos_buildflags.h"
#include "media/capture/mojom/image_capture_types.h"
#include "media/capture/video/mock_device.h"
#include "media/capture/video/mock_device_factory.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video/video_capture_system_impl.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/device_factory_impl.h"
#include "services/video_capture/device_media_to_mojo_adapter.h"
#include "services/video_capture/public/cpp/mock_video_frame_handler.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"
#include "services/video_capture/public/mojom/video_frame_handler.mojom.h"
#include "services/video_capture/public/mojom/video_source.mojom.h"
#include "services/video_capture/video_source_provider_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::SaveArg;

namespace video_capture {

class MockVideoCaptureDeviceSharedAccessTest : public ::testing::Test {
 public:
  MockVideoCaptureDeviceSharedAccessTest()
      : mock_video_frame_handler_1_(
            video_frame_handler_1_.InitWithNewPipeAndPassReceiver()),
        mock_video_frame_handler_2_(
            video_frame_handler_2_.InitWithNewPipeAndPassReceiver()),
        next_arbitrary_frame_feedback_id_(123) {}
  ~MockVideoCaptureDeviceSharedAccessTest() override {}

  void SetUp() override {
    auto mock_device_factory = std::make_unique<media::MockDeviceFactory>();
    mock_device_factory_ = mock_device_factory.get();
    media::VideoCaptureDeviceDescriptor mock_descriptor;
    mock_descriptor.device_id = "MockDeviceId";
    mock_device_factory->AddMockDevice(&mock_device_, mock_descriptor);

    auto video_capture_system = std::make_unique<media::VideoCaptureSystemImpl>(
        std::move(mock_device_factory));
#if BUILDFLAG(IS_CHROMEOS_ASH)
    service_device_factory_ = std::make_unique<DeviceFactoryImpl>(
        std::move(video_capture_system), base::DoNothing(),
        base::SingleThreadTaskRunner::GetCurrentDefault());
#else
    service_device_factory_ =
        std::make_unique<DeviceFactoryImpl>(std::move(video_capture_system));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    source_provider_ = std::make_unique<VideoSourceProviderImpl>(
        service_device_factory_.get(), base::DoNothing());

    // Obtain the mock device backed source from |source_provider_|.
    base::MockCallback<mojom::VideoSourceProvider::GetSourceInfosCallback>
        source_infos_receiver;
    base::RunLoop wait_loop;
    EXPECT_CALL(source_infos_receiver, Run)
        .WillOnce(InvokeWithoutArgs([&wait_loop]() { wait_loop.Quit(); }));
    source_provider_->GetSourceInfos(source_infos_receiver.Get());
    // We must wait for the response to GetDeviceInfos before calling
    // CreateDevice.
    wait_loop.Run();
    source_provider_->GetVideoSource(
        mock_descriptor.device_id,
        source_remote_1_.BindNewPipeAndPassReceiver());
    source_provider_->GetVideoSource(
        mock_descriptor.device_id,
        source_remote_2_.BindNewPipeAndPassReceiver());

    requestable_settings_.requested_format.frame_size = gfx::Size(800, 600);
    requestable_settings_.requested_format.frame_rate = 15;
    requestable_settings_.requested_format.pixel_format =
        media::PIXEL_FORMAT_I420;
    requestable_settings_.resolution_change_policy =
        media::ResolutionChangePolicy::FIXED_RESOLUTION;
    requestable_settings_.power_line_frequency =
        media::PowerLineFrequency::kDefault;
  }

  void LetClient1ConnectWithRequestableSettingsAndExpectToGetThem() {
    base::RunLoop run_loop;
    source_remote_1_->CreatePushSubscription(
        std::move(video_frame_handler_1_), requestable_settings_,
        false /*force_reopen_with_new_settings*/,
        subscription_1_.BindNewPipeAndPassReceiver(),
        base::BindOnce(
            [](base::RunLoop* run_loop,
               media::VideoCaptureParams* requested_settings,
               mojom::CreatePushSubscriptionResultCodePtr result_code,
               const media::VideoCaptureParams&
                   settings_source_was_opened_with) {
              ASSERT_EQ(mojom::CreatePushSubscriptionSuccessCode::
                            kCreatedWithRequestedSettings,
                        result_code->get_success_code());
              ASSERT_EQ(*requested_settings, settings_source_was_opened_with);
              run_loop->Quit();
            },
            &run_loop, &requestable_settings_));
    run_loop.Run();
  }

  void LetClient2ConnectWithRequestableSettingsAndExpectToGetThem() {
    LetClient2ConnectWithRequestableSettings(
        false /*force_reopen_with_new_settings*/,
        mojom::CreatePushSubscriptionResultCode::NewSuccessCode(
            mojom::CreatePushSubscriptionSuccessCode::
                kCreatedWithRequestedSettings));
  }

  void LetClient2ConnectWithRequestableSettings(
      bool force_reopen_with_new_settings,
      mojom::CreatePushSubscriptionResultCodePtr expected_result_code) {
    base::RunLoop run_loop;
    source_remote_2_->CreatePushSubscription(
        std::move(video_frame_handler_2_), requestable_settings_,
        force_reopen_with_new_settings,
        subscription_2_.BindNewPipeAndPassReceiver(),
        base::BindOnce(
            [](base::RunLoop* run_loop,
               media::VideoCaptureParams* requested_settings,
               mojom::CreatePushSubscriptionResultCodePtr expected_result_code,
               mojom::CreatePushSubscriptionResultCodePtr result_code,
               const media::VideoCaptureParams&
                   settings_source_was_opened_with) {
              ASSERT_EQ(expected_result_code, result_code);
              if (expected_result_code->is_success_code()) {
                mojom::CreatePushSubscriptionSuccessCode success_code =
                    expected_result_code->get_success_code();
                if (success_code == mojom::CreatePushSubscriptionSuccessCode::
                                        kCreatedWithRequestedSettings) {
                  ASSERT_EQ(*requested_settings,
                            settings_source_was_opened_with);
                }
                if (success_code == mojom::CreatePushSubscriptionSuccessCode::
                                        kCreatedWithDifferentSettings) {
                  ASSERT_FALSE(*requested_settings ==
                               settings_source_was_opened_with);
                }
              }
              run_loop->Quit();
            },
            &run_loop, &requestable_settings_,
            std::move(expected_result_code)));
    run_loop.Run();
  }

  void LetTwoClientsConnectWithSameSettingsOneByOne() {
    LetClient1ConnectWithRequestableSettingsAndExpectToGetThem();
    LetClient2ConnectWithRequestableSettingsAndExpectToGetThem();
  }

  void LetTwoClientsConnectWithDifferentSettings() {
    base::RunLoop run_loop_1;
    base::RunLoop run_loop_2;
    source_remote_1_->CreatePushSubscription(
        std::move(video_frame_handler_1_), requestable_settings_,
        false /*force_reopen_with_new_settings*/,
        subscription_1_.BindNewPipeAndPassReceiver(),
        base::BindOnce(
            [](base::RunLoop* run_loop_1,
               media::VideoCaptureParams* requested_settings,
               mojom::CreatePushSubscriptionResultCodePtr result_code,
               const media::VideoCaptureParams&
                   settings_source_was_opened_with) {
              ASSERT_EQ(mojom::CreatePushSubscriptionSuccessCode::
                            kCreatedWithRequestedSettings,
                        result_code->get_success_code());
              ASSERT_EQ(*requested_settings, settings_source_was_opened_with);
              run_loop_1->Quit();
            },
            &run_loop_1, &requestable_settings_));

    auto different_settings = requestable_settings_;
    // Change something arbitrary
    different_settings.requested_format.frame_size = gfx::Size(124, 456);
    ASSERT_FALSE(requestable_settings_ == different_settings);

    source_remote_2_->CreatePushSubscription(
        std::move(video_frame_handler_2_), different_settings,
        false /*force_reopen_with_new_settings*/,
        subscription_2_.BindNewPipeAndPassReceiver(),
        base::BindOnce(
            [](base::RunLoop* run_loop_2,
               media::VideoCaptureParams* requested_settings,
               mojom::CreatePushSubscriptionResultCodePtr result_code,
               const media::VideoCaptureParams&
                   settings_source_was_opened_with) {
              ASSERT_EQ(mojom::CreatePushSubscriptionSuccessCode::
                            kCreatedWithDifferentSettings,
                        result_code->get_success_code());
              ASSERT_EQ(*requested_settings, settings_source_was_opened_with);
              run_loop_2->Quit();
            },
            &run_loop_2, &requestable_settings_));

    run_loop_1.Run();
    run_loop_2.Run();
  }

  void SendFrameAndExpectToArriveAtBothSubscribers() {
    const int32_t kArbitraryFrameFeedbackId =
        next_arbitrary_frame_feedback_id_++;
    const int32_t kArbitraryRotation = 0;
    base::RunLoop wait_loop_1;
    EXPECT_CALL(mock_video_frame_handler_1_,
                DoOnFrameReadyInBuffer(_, kArbitraryFrameFeedbackId, _))
        .WillOnce(InvokeWithoutArgs([&wait_loop_1]() { wait_loop_1.Quit(); }));
    base::RunLoop wait_loop_2;
    EXPECT_CALL(mock_video_frame_handler_2_,
                DoOnFrameReadyInBuffer(_, kArbitraryFrameFeedbackId, _))
        .WillOnce(InvokeWithoutArgs([&wait_loop_2]() { wait_loop_2.Quit(); }));
    mock_device_.SendStubFrame(requestable_settings_.requested_format,
                               kArbitraryRotation, kArbitraryFrameFeedbackId);
    wait_loop_1.Run();
    wait_loop_2.Run();
    Mock::VerifyAndClearExpectations(&mock_video_frame_handler_1_);
    Mock::VerifyAndClearExpectations(&mock_video_frame_handler_2_);
  }

  void SendFrameAndExpectToArriveOnlyAtSubscriber1() {
    const int32_t kArbitraryFrameFeedbackId =
        next_arbitrary_frame_feedback_id_++;
    const int32_t kArbitraryRotation = 0;

    base::RunLoop wait_loop;
    EXPECT_CALL(mock_video_frame_handler_1_,
                DoOnFrameReadyInBuffer(_, kArbitraryFrameFeedbackId, _))
        .WillOnce(InvokeWithoutArgs([&wait_loop]() { wait_loop.Quit(); }));
    EXPECT_CALL(mock_video_frame_handler_2_, DoOnFrameReadyInBuffer(_, _, _))
        .Times(0);
    mock_device_.SendStubFrame(requestable_settings_.requested_format,
                               kArbitraryRotation, kArbitraryFrameFeedbackId);
    wait_loop.Run();
    Mock::VerifyAndClearExpectations(&mock_video_frame_handler_1_);
    Mock::VerifyAndClearExpectations(&mock_video_frame_handler_2_);
  }

  void SendFrameAndExpectToArriveOnlyAtSubscriber2() {
    const int32_t kArbitraryFrameFeedbackId =
        next_arbitrary_frame_feedback_id_++;
    const int32_t kArbitraryRotation = 0;

    base::RunLoop wait_loop;
    EXPECT_CALL(mock_video_frame_handler_1_, DoOnFrameReadyInBuffer(_, _, _))
        .Times(0);
    EXPECT_CALL(mock_video_frame_handler_2_,
                DoOnFrameReadyInBuffer(_, kArbitraryFrameFeedbackId, _))
        .WillOnce(InvokeWithoutArgs([&wait_loop]() { wait_loop.Quit(); }));
    mock_device_.SendStubFrame(requestable_settings_.requested_format,
                               kArbitraryRotation, kArbitraryFrameFeedbackId);
    wait_loop.Run();
    Mock::VerifyAndClearExpectations(&mock_video_frame_handler_1_);
    Mock::VerifyAndClearExpectations(&mock_video_frame_handler_2_);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  media::MockDevice mock_device_;
  raw_ptr<media::MockDeviceFactory, DanglingUntriaged> mock_device_factory_;
  std::unique_ptr<DeviceFactoryImpl> service_device_factory_;
  std::unique_ptr<VideoSourceProviderImpl> source_provider_;
  mojo::Remote<mojom::VideoSource> source_remote_1_;
  mojo::Remote<mojom::VideoSource> source_remote_2_;
  media::VideoCaptureParams requestable_settings_;

  mojo::Remote<mojom::PushVideoStreamSubscription> subscription_1_;
  mojo::PendingRemote<mojom::VideoFrameHandler> video_frame_handler_1_;
  MockVideoFrameHandler mock_video_frame_handler_1_;
  mojo::Remote<mojom::PushVideoStreamSubscription> subscription_2_;
  mojo::PendingRemote<mojom::VideoFrameHandler> video_frame_handler_2_;
  MockVideoFrameHandler mock_video_frame_handler_2_;

  int32_t next_arbitrary_frame_feedback_id_;

 private:
};

TEST_F(MockVideoCaptureDeviceSharedAccessTest,
       TwoClientsCreatePushSubscriptionWithSameSettings) {
  LetTwoClientsConnectWithSameSettingsOneByOne();
}

TEST_F(MockVideoCaptureDeviceSharedAccessTest,
       TwoClientsCreatePushSubscriptionWithDifferentSettings) {
  LetTwoClientsConnectWithDifferentSettings();
}

TEST_F(MockVideoCaptureDeviceSharedAccessTest,
       SecondClientForcesReopenWithDifferentSettings) {
  LetClient1ConnectWithRequestableSettingsAndExpectToGetThem();
  subscription_1_->Activate();

  auto previously_requested_settings = requestable_settings_;
  // Change something arbitrary
  requestable_settings_.requested_format.frame_size = gfx::Size(124, 456);
  ASSERT_FALSE(requestable_settings_ == previously_requested_settings);

  LetClient2ConnectWithRequestableSettings(
      true /*force_reopen_with_new_settings*/,
      mojom::CreatePushSubscriptionResultCode::NewSuccessCode(
          mojom::CreatePushSubscriptionSuccessCode::
              kCreatedWithRequestedSettings));
  subscription_2_->Activate();
  SendFrameAndExpectToArriveAtBothSubscribers();
}

TEST_F(MockVideoCaptureDeviceSharedAccessTest,
       SecondClientsForcesReopenWithSameSettings) {
  LetClient1ConnectWithRequestableSettingsAndExpectToGetThem();
  LetClient2ConnectWithRequestableSettings(
      true /*force_reopen_with_new_settings*/,
      mojom::CreatePushSubscriptionResultCode::NewSuccessCode(
          mojom::CreatePushSubscriptionSuccessCode::
              kCreatedWithRequestedSettings));
  subscription_1_->Activate();
  subscription_2_->Activate();
  SendFrameAndExpectToArriveAtBothSubscribers();
}

// Tests that existing buffers are retired but no OnStopped() and OnStarted()
// event is sent to existing client when the device internally restarts because
// a new client connects with |force_reopen_with_new_settings| set to true.
TEST_F(MockVideoCaptureDeviceSharedAccessTest,
       InternalDeviceRestartIsTransparentToExistingSubscribers) {
  LetClient1ConnectWithRequestableSettingsAndExpectToGetThem();
  EXPECT_CALL(mock_video_frame_handler_1_, DoOnNewBuffer(_, _)).Times(1);
  EXPECT_CALL(mock_video_frame_handler_1_, OnStarted()).Times(1);
  subscription_1_->Activate();
  mock_device_.SendOnStarted();
  SendFrameAndExpectToArriveOnlyAtSubscriber1();
  Mock::VerifyAndClearExpectations(&mock_video_frame_handler_1_);

  auto previously_requested_settings = requestable_settings_;
  // Change something arbitrary
  requestable_settings_.requested_format.frame_size = gfx::Size(124, 456);
  ASSERT_FALSE(requestable_settings_ == previously_requested_settings);

  {
    testing::InSequence s;
    EXPECT_CALL(mock_video_frame_handler_1_, DoOnBufferRetired(_)).Times(1);
    EXPECT_CALL(mock_video_frame_handler_1_, DoOnNewBuffer(_, _)).Times(1);
  }
  EXPECT_CALL(mock_video_frame_handler_1_, OnStopped()).Times(0);
  EXPECT_CALL(mock_video_frame_handler_1_, OnStarted()).Times(0);

  LetClient2ConnectWithRequestableSettings(
      true /*force_reopen_with_new_settings*/,
      mojom::CreatePushSubscriptionResultCode::NewSuccessCode(
          mojom::CreatePushSubscriptionSuccessCode::
              kCreatedWithRequestedSettings));
  subscription_2_->Activate();

  mock_device_.SendOnStarted();
  SendFrameAndExpectToArriveAtBothSubscribers();
  Mock::VerifyAndClearExpectations(&mock_video_frame_handler_1_);
}

TEST_F(MockVideoCaptureDeviceSharedAccessTest,
       CreatingSubscriptionFailsWhenCreatingDeviceFails) {
  // Make it so that attempts to open the mock device will fail.
  mock_device_factory_->RemoveAllDevices();

  LetClient2ConnectWithRequestableSettings(
      false /*force_reopen_with_new_settings*/,
      mojom::CreatePushSubscriptionResultCode::NewErrorCode(
          media::VideoCaptureError::kVideoCaptureSystemDeviceIdNotFound));
}

TEST_F(MockVideoCaptureDeviceSharedAccessTest,
       NoFramesArePushedUntilSubscriptionIsActivated) {
  LetTwoClientsConnectWithSameSettingsOneByOne();

  subscription_2_->Activate();
  SendFrameAndExpectToArriveOnlyAtSubscriber2();

  subscription_1_->Activate();
  SendFrameAndExpectToArriveAtBothSubscribers();
}

TEST_F(MockVideoCaptureDeviceSharedAccessTest,
       DiscardingLastSubscriptionStopsTheDevice) {
  EXPECT_CALL(mock_device_, DoAllocateAndStart(_, _)).Times(1);
  EXPECT_CALL(mock_device_, DoStopAndDeAllocate()).Times(0);
  LetTwoClientsConnectWithDifferentSettings();
  subscription_1_.reset();
  Mock::VerifyAndClearExpectations(&mock_device_);

  base::RunLoop wait_loop;
  EXPECT_CALL(mock_device_, DoStopAndDeAllocate())
      .WillOnce(InvokeWithoutArgs([&wait_loop]() { wait_loop.Quit(); }));
  subscription_2_.reset();
  wait_loop.Run();

  // DeviceMediaToMojoAdapter::Stop() issues a DeleteSoon for its
  // |video_frame_handler_| on the current sequence. Wait for this before
  // exiting the test in order to avoid leaked object failing ASAN tests. See
  // also  https://crbug.com/961066.
  base::RunLoop().RunUntilIdle();
}

TEST_F(MockVideoCaptureDeviceSharedAccessTest,
       DiscardingLastVideoSourceRemoteStopsTheDevice) {
  LetTwoClientsConnectWithDifferentSettings();
  subscription_1_->Activate();
  subscription_2_->Activate();

  testing::MockFunction<void()> source_remote_1_reset_marker;
  testing::Expectation source_remote_1_reset_expectation =
      EXPECT_CALL(source_remote_1_reset_marker, Call).Times(1);
  source_remote_1_.reset();
  // Spin to allow mojo calls to be serviced.
  base::RunLoop().RunUntilIdle();
  source_remote_1_reset_marker.Call();

  base::RunLoop wait_loop;
  EXPECT_CALL(mock_device_, DoStopAndDeAllocate())
      .After(source_remote_1_reset_expectation)
      .WillOnce(InvokeWithoutArgs([&wait_loop]() { wait_loop.Quit(); }));
  source_remote_2_.reset();
  wait_loop.Run();

  // DeviceMediaToMojoAdapter::Stop() issues a DeleteSoon for its
  // |video_frame_handler_| on the current sequence. Wait for this before
  // exiting the test in order to avoid leaked object failing ASAN tests.
  // See also  https://crbug.com/961066.
  base::RunLoop().RunUntilIdle();
}

TEST_F(MockVideoCaptureDeviceSharedAccessTest,
       NoMoreFramesArriveAfterClosingSubscription) {
  LetTwoClientsConnectWithDifferentSettings();
  subscription_1_->Activate();
  subscription_2_->Activate();

  SendFrameAndExpectToArriveAtBothSubscribers();

  {
    base::RunLoop wait_loop;
    subscription_1_->Close(base::BindOnce(
        [](base::RunLoop* wait_loop) { wait_loop->Quit(); }, &wait_loop));
    wait_loop.Run();
  }

  SendFrameAndExpectToArriveOnlyAtSubscriber2();
}

TEST_F(MockVideoCaptureDeviceSharedAccessTest, SuspendAndResume) {
  LetTwoClientsConnectWithDifferentSettings();
  subscription_1_->Activate();
  subscription_2_->Activate();

  SendFrameAndExpectToArriveAtBothSubscribers();

  {
    base::RunLoop wait_loop;
    subscription_1_->Suspend(base::BindOnce(
        [](base::RunLoop* wait_loop) { wait_loop->Quit(); }, &wait_loop));
    wait_loop.Run();
  }

  SendFrameAndExpectToArriveOnlyAtSubscriber2();

  subscription_1_->Resume();
  SendFrameAndExpectToArriveAtBothSubscribers();
}

TEST_F(MockVideoCaptureDeviceSharedAccessTest, SuspendAndResumeSingleClient) {
  LetClient1ConnectWithRequestableSettingsAndExpectToGetThem();
  subscription_1_->Activate();
  {
    base::RunLoop wait_loop;
    subscription_1_->Suspend(base::BindOnce(
        [](base::RunLoop* wait_loop) { wait_loop->Quit(); }, &wait_loop));
    wait_loop.Run();
  }
  EXPECT_CALL(mock_video_frame_handler_1_, DoOnFrameReadyInBuffer(_, _, _))
      .Times(0);

  // Send a couple of frames. We want to send at least as many frames as
  // the maximum buffer count in the video frame pool to make sure that
  // buffers are properly released and reused.
  mock_device_.SendOnStarted();
  for (int i = 0; i < DeviceMediaToMojoAdapter::max_buffer_pool_buffer_count();
       i++) {
    const int32_t kArbitraryRotation = 0;
    const int32_t kArbitraryFrameFeedbackId =
        next_arbitrary_frame_feedback_id_++;
    mock_device_.SendStubFrame(requestable_settings_.requested_format,
                               kArbitraryRotation, kArbitraryFrameFeedbackId);
    // We need to wait until the frame has arrived at BroadcastingReceiver
    base::RunLoop().RunUntilIdle();
  }
  Mock::VerifyAndClearExpectations(&mock_video_frame_handler_1_);

  subscription_1_->Resume();
  subscription_1_.FlushForTesting();
  SendFrameAndExpectToArriveOnlyAtSubscriber1();
}

TEST_F(MockVideoCaptureDeviceSharedAccessTest,
       CreateNewSubscriptionAfterClosingExistingOneUsesNewSettings) {
  LetClient1ConnectWithRequestableSettingsAndExpectToGetThem();
  base::RunLoop run_loop;
  subscription_1_->Close(base::BindOnce(
      [](base::RunLoop* run_loop) { run_loop->Quit(); }, &run_loop));
  run_loop.Run();

  // modify settings to check if they will be applied
  const int kArbitraryDifferentWidth = 753;
  requestable_settings_.requested_format.frame_size.set_width(
      kArbitraryDifferentWidth);
  LetClient2ConnectWithRequestableSettingsAndExpectToGetThem();
}

TEST_F(MockVideoCaptureDeviceSharedAccessTest,
       TakePhotoUsingOnPushSubscriptionWithDifferentSettings) {
  LetTwoClientsConnectWithDifferentSettings();

  {
    EXPECT_CALL(mock_device_, DoGetPhotoState(_))
        .WillOnce(Invoke(
            [](media::VideoCaptureDevice::GetPhotoStateCallback* callback) {
              media::mojom::PhotoStatePtr state = mojo::CreateEmptyPhotoState();
              std::move(*callback).Run(std::move(state));
            }));
    base::RunLoop run_loop;
    subscription_1_->GetPhotoState(base::BindOnce(
        [](base::RunLoop* run_loop, media::mojom::PhotoStatePtr state) {
          run_loop->Quit();
        },
        &run_loop));

    run_loop.Run();
  }

  {
    EXPECT_CALL(mock_device_, DoSetPhotoOptions(_, _))
        .WillOnce(Invoke(
            [](media::mojom::PhotoSettingsPtr* settings,
               media::VideoCaptureDevice::SetPhotoOptionsCallback* callback) {
              media::mojom::BlobPtr blob = media::mojom::Blob::New();
              std::move(*callback).Run(true);
            }));
    media::mojom::PhotoSettingsPtr settings =
        media::mojom::PhotoSettings::New();
    base::RunLoop run_loop;
    subscription_1_->SetPhotoOptions(
        std::move(settings), base::BindOnce(
                                 [](base::RunLoop* run_loop, bool succeeded) {
                                   ASSERT_TRUE(succeeded);
                                   run_loop->Quit();
                                 },
                                 &run_loop));
    run_loop.Run();
  }

  {
    EXPECT_CALL(mock_device_, DoTakePhoto(_))
        .WillOnce(
            Invoke([](media::VideoCaptureDevice::TakePhotoCallback* callback) {
              media::mojom::BlobPtr blob = media::mojom::Blob::New();
              std::move(*callback).Run(std::move(blob));
            }));
    base::RunLoop run_loop;
    subscription_1_->TakePhoto(
        base::BindOnce([](base::RunLoop* run_loop,
                          media::mojom::BlobPtr state) { run_loop->Quit(); },
                       &run_loop));

    run_loop.Run();
  }
}

TEST_F(MockVideoCaptureDeviceSharedAccessTest,
       TakePhotoUsingOnPushSubscriptionWithSameSetting) {
  LetTwoClientsConnectWithSameSettingsOneByOne();

  {
    EXPECT_CALL(mock_device_, DoGetPhotoState(_))
        .WillOnce(Invoke(
            [](media::VideoCaptureDevice::GetPhotoStateCallback* callback) {
              media::mojom::PhotoStatePtr state = mojo::CreateEmptyPhotoState();
              std::move(*callback).Run(std::move(state));
            }));
    base::RunLoop run_loop;
    subscription_1_->GetPhotoState(base::BindOnce(
        [](base::RunLoop* run_loop, media::mojom::PhotoStatePtr state) {
          run_loop->Quit();
        },
        &run_loop));

    run_loop.Run();
  }

  {
    EXPECT_CALL(mock_device_, DoSetPhotoOptions(_, _))
        .WillOnce(Invoke(
            [](media::mojom::PhotoSettingsPtr* settings,
               media::VideoCaptureDevice::SetPhotoOptionsCallback* callback) {
              media::mojom::BlobPtr blob = media::mojom::Blob::New();
              std::move(*callback).Run(true);
            }));
    media::mojom::PhotoSettingsPtr settings =
        media::mojom::PhotoSettings::New();
    base::RunLoop run_loop;
    subscription_1_->SetPhotoOptions(
        std::move(settings), base::BindOnce(
                                 [](base::RunLoop* run_loop, bool succeeded) {
                                   ASSERT_TRUE(succeeded);
                                   run_loop->Quit();
                                 },
                                 &run_loop));
    run_loop.Run();
  }

  {
    EXPECT_CALL(mock_device_, DoTakePhoto(_))
        .WillOnce(
            Invoke([](media::VideoCaptureDevice::TakePhotoCallback* callback) {
              media::mojom::BlobPtr blob = media::mojom::Blob::New();
              std::move(*callback).Run(std::move(blob));
            }));
    base::RunLoop run_loop;
    subscription_1_->TakePhoto(
        base::BindOnce([](base::RunLoop* run_loop,
                          media::mojom::BlobPtr state) { run_loop->Quit(); },
                       &run_loop));

    run_loop.Run();
  }

  {
    EXPECT_CALL(mock_device_, DoGetPhotoState(_))
        .WillOnce(Invoke(
            [](media::VideoCaptureDevice::GetPhotoStateCallback* callback) {
              media::mojom::PhotoStatePtr state = mojo::CreateEmptyPhotoState();
              std::move(*callback).Run(std::move(state));
            }));
    base::RunLoop run_loop;
    subscription_2_->GetPhotoState(base::BindOnce(
        [](base::RunLoop* run_loop, media::mojom::PhotoStatePtr state) {
          run_loop->Quit();
        },
        &run_loop));

    run_loop.Run();
  }

  {
    EXPECT_CALL(mock_device_, DoSetPhotoOptions(_, _))
        .WillOnce(Invoke(
            [](media::mojom::PhotoSettingsPtr* settings,
               media::VideoCaptureDevice::SetPhotoOptionsCallback* callback) {
              media::mojom::BlobPtr blob = media::mojom::Blob::New();
              std::move(*callback).Run(true);
            }));
    media::mojom::PhotoSettingsPtr settings =
        media::mojom::PhotoSettings::New();
    base::RunLoop run_loop;
    subscription_2_->SetPhotoOptions(
        std::move(settings), base::BindOnce(
                                 [](base::RunLoop* run_loop, bool succeeded) {
                                   ASSERT_TRUE(succeeded);
                                   run_loop->Quit();
                                 },
                                 &run_loop));
    run_loop.Run();
  }

  {
    EXPECT_CALL(mock_device_, DoTakePhoto(_))
        .WillOnce(
            Invoke([](media::VideoCaptureDevice::TakePhotoCallback* callback) {
              media::mojom::BlobPtr blob = media::mojom::Blob::New();
              std::move(*callback).Run(std::move(blob));
            }));
    base::RunLoop run_loop;
    subscription_2_->TakePhoto(
        base::BindOnce([](base::RunLoop* run_loop,
                          media::mojom::BlobPtr state) { run_loop->Quit(); },
                       &run_loop));

    run_loop.Run();
  }
}

}  // namespace video_capture
