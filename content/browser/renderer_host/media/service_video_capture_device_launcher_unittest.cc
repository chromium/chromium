// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/service_video_capture_device_launcher.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/mock_callback.h"
#include "base/threading/thread.h"
#include "content/browser/renderer_host/media/ref_counted_video_source_provider.h"
#include "content/browser/renderer_host/media/service_launched_video_capture_device.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/video_capture/public/cpp/mock_push_subscription.h"
#include "services/video_capture/public/cpp/mock_video_source.h"
#include "services/video_capture/public/cpp/mock_video_source_provider.h"
#include "services/video_capture/public/mojom/video_frame_handler.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::InvokeWithoutArgs;

namespace content {

static const std::string kStubDeviceId = "StubDevice";
static const media::VideoCaptureParams kArbitraryParams;
static const base::WeakPtr<media::VideoFrameReceiver> kNullReceiver;

class MockVideoCaptureDeviceLauncherCallbacks
    : public VideoCaptureDeviceLauncher::Callbacks {
 public:
  void OnDeviceLaunched(
      std::unique_ptr<LaunchedVideoCaptureDevice> device) override {
    DoOnDeviceLaunched(&device);
  }

  MOCK_METHOD1(DoOnDeviceLaunched,
               void(std::unique_ptr<LaunchedVideoCaptureDevice>* device));
  MOCK_METHOD1(OnDeviceLaunchFailed, void(media::VideoCaptureError error));
  MOCK_METHOD0(OnDeviceLaunchAborted, void());
};

class ServiceVideoCaptureDeviceLauncherTest : public testing::Test {
 public:
  ServiceVideoCaptureDeviceLauncherTest() {}

  ServiceVideoCaptureDeviceLauncherTest(
      const ServiceVideoCaptureDeviceLauncherTest&) = delete;
  ServiceVideoCaptureDeviceLauncherTest& operator=(
      const ServiceVideoCaptureDeviceLauncherTest&) = delete;

  ~ServiceVideoCaptureDeviceLauncherTest() override {}

  void CloseSourceReceiver() { source_receiver_.reset(); }

  void CloseSubscriptionReceivers() { subscription_receivers_.Clear(); }

 protected:
  void SetUp() override {
    source_provider_receiver_ = std::make_unique<
        mojo::Receiver<video_capture::mojom::VideoSourceProvider>>(
        &mock_source_provider_, source_provider_.BindNewPipeAndPassReceiver());
    service_connection_ = base::MakeRefCounted<RefCountedVideoSourceProvider>(
        std::move(source_provider_), release_connection_cb_.Get());

    launcher_ = std::make_unique<ServiceVideoCaptureDeviceLauncher>(
        connect_to_device_factory_cb_.Get());
    launcher_has_connected_to_source_provider_ = false;
    launcher_has_released_source_provider_ = false;

    ON_CALL(connect_to_device_factory_cb_, Run(_))
        .WillByDefault(Invoke(
            [this](scoped_refptr<RefCountedVideoSourceProvider>* out_provider) {
              launcher_has_connected_to_source_provider_ = true;
              *out_provider = service_connection_;
            }));

    ON_CALL(release_connection_cb_, Run())
        .WillByDefault(InvokeWithoutArgs([this]() {
          launcher_has_released_source_provider_ = true;
          wait_for_release_connection_cb_.Quit();
        }));

    ON_CALL(mock_source_provider_, DoGetVideoSource(kStubDeviceId, _))
        .WillByDefault(Invoke(
            [this](const std::string& device_id,
                   mojo::PendingReceiver<video_capture::mojom::VideoSource>*
                       source_receiver) {
              source_receiver_ = std::make_unique<
                  mojo::Receiver<video_capture::mojom::VideoSource>>(
                  &mock_source_, std::move(*source_receiver));
            }));

    ON_CALL(mock_source_, CreatePushSubscription(_, _, _, _, _))
        .WillByDefault(Invoke(
            [this](mojo::PendingRemote<video_capture::mojom::VideoFrameHandler>
                       subscriber,
                   const media::VideoCaptureParams& requested_settings,
                   bool force_reopen_with_new_settings,
                   mojo::PendingReceiver<
                       video_capture::mojom::PushVideoStreamSubscription>
                       subscription,
                   video_capture::mojom::VideoSource::
                       CreatePushSubscriptionCallback callback) {
              subscription_receivers_.Add(&mock_subscription_,
                                          std::move(subscription));
              std::move(callback).Run(
                  video_capture::mojom::CreatePushSubscriptionResultCode::
                      NewSuccessCode(video_capture::mojom::
                                         CreatePushSubscriptionSuccessCode::
                                             kCreatedWithRequestedSettings),
                  requested_settings);
            }));
  }

  void TearDown() override {}

  void RunLaunchingDeviceIsAbortedTest(
      video_capture::mojom::CreatePushSubscriptionResultCodePtr
          service_result_code);
  void RunConnectionLostAfterSuccessfulStartTest(
      base::OnceClosure close_connection_cb);

  BrowserTaskEnvironment task_environment_;
  MockVideoCaptureDeviceLauncherCallbacks mock_callbacks_;
  mojo::Remote<video_capture::mojom::VideoSourceProvider> source_provider_;
  video_capture::MockVideoSourceProvider mock_source_provider_;
  std::unique_ptr<mojo::Receiver<video_capture::mojom::VideoSourceProvider>>
      source_provider_receiver_;
  video_capture::MockVideoSource mock_source_;
  std::unique_ptr<mojo::Receiver<video_capture::mojom::VideoSource>>
      source_receiver_;
  video_capture::MockPushSubcription mock_subscription_;
  mojo::ReceiverSet<video_capture::mojom::PushVideoStreamSubscription>
      subscription_receivers_;
  std::unique_ptr<ServiceVideoCaptureDeviceLauncher> launcher_;
  base::MockCallback<base::OnceClosure> connection_lost_cb_;
  base::MockCallback<base::OnceClosure> done_cb_;
  base::MockCallback<
      ServiceVideoCaptureDeviceLauncher::ConnectToDeviceFactoryCB>
      connect_to_device_factory_cb_;
  base::MockCallback<base::OnceClosure> release_connection_cb_;
  bool launcher_has_connected_to_source_provider_;
  bool launcher_has_released_source_provider_;
  base::RunLoop wait_for_release_connection_cb_;
  // Destroy `service_connection_` before everything else; destroying the
  // `RefCountedVideoSourceProvider` can end up referencing other fields of the
  // test fixture.
  scoped_refptr<RefCountedVideoSourceProvider> service_connection_;
};

TEST_F(ServiceVideoCaptureDeviceLauncherTest, LaunchingDeviceSucceeds) {
  EXPECT_CALL(mock_callbacks_, DoOnDeviceLaunched(_)).Times(1);
  EXPECT_CALL(mock_callbacks_, OnDeviceLaunchAborted()).Times(0);
  EXPECT_CALL(mock_callbacks_, OnDeviceLaunchFailed(_)).Times(0);
  EXPECT_CALL(connection_lost_cb_, Run()).Times(0);
  base::RunLoop wait_for_done_cb;
  EXPECT_CALL(done_cb_, Run())
      .WillOnce(InvokeWithoutArgs(
          [&wait_for_done_cb]() { wait_for_done_cb.Quit(); }));

  // Exercise
  launcher_->LaunchDeviceAsync(
      kStubDeviceId, blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
      kArbitraryParams, kNullReceiver, connection_lost_cb_.Get(),
      &mock_callbacks_, done_cb_.Get(), {});
  wait_for_done_cb.Run();

  launcher_.reset();
  service_connection_.reset();
  wait_for_release_connection_cb_.Run();
  EXPECT_TRUE(launcher_has_connected_to_source_provider_);
  EXPECT_TRUE(launcher_has_released_source_provider_);
}

TEST_F(ServiceVideoCaptureDeviceLauncherTest,
       LaunchingDeviceIsAbortedBeforeServiceRespondsWithSuccess) {
  RunLaunchingDeviceIsAbortedTest(
      video_capture::mojom::CreatePushSubscriptionResultCode::NewSuccessCode(
          video_capture::mojom::CreatePushSubscriptionSuccessCode::
              kCreatedWithRequestedSettings));
}

TEST_F(ServiceVideoCaptureDeviceLauncherTest,
       LaunchingDeviceIsAbortedBeforeServiceRespondsWithNotFound) {
  RunLaunchingDeviceIsAbortedTest(
      video_capture::mojom::CreatePushSubscriptionResultCode::NewSuccessCode(
          video_capture::mojom::CreatePushSubscriptionSuccessCode::
              kCreatedWithDifferentSettings));
}

TEST_F(ServiceVideoCaptureDeviceLauncherTest,
       LaunchingDeviceIsAbortedBeforeServiceRespondsWithNotInitialized) {
  RunLaunchingDeviceIsAbortedTest(
      video_capture::mojom::CreatePushSubscriptionResultCode::NewErrorCode(
          media::VideoCaptureError::kIntentionalErrorRaisedByUnitTest));
}

void ServiceVideoCaptureDeviceLauncherTest::RunLaunchingDeviceIsAbortedTest(
    video_capture::mojom::CreatePushSubscriptionResultCodePtr
        service_result_code) {
  base::RunLoop step_1_run_loop;
  base::RunLoop step_2_run_loop;

  base::OnceClosure create_push_subscription_success_answer_cb;
  EXPECT_CALL(mock_source_, CreatePushSubscription(_, _, _, _, _))
      .WillOnce(Invoke(
          [&create_push_subscription_success_answer_cb, &step_1_run_loop,
           &service_result_code](
              mojo::PendingRemote<video_capture::mojom::VideoFrameHandler>
                  subscriber,
              const media::VideoCaptureParams& requested_settings,
              bool force_reopen_with_new_settings,
              mojo::PendingReceiver<
                  video_capture::mojom::PushVideoStreamSubscription>
                  subscription,
              video_capture::mojom::VideoSource::CreatePushSubscriptionCallback
                  callback) {
            // Prepare the callback, but save it for now instead of invoking it.
            create_push_subscription_success_answer_cb = base::BindOnce(
                [](const media::VideoCaptureParams& requested_settings,
                   mojo::PendingReceiver<
                       video_capture::mojom::PushVideoStreamSubscription>
                       subscription,
                   video_capture::mojom::VideoSource::
                       CreatePushSubscriptionCallback callback,
                   video_capture::mojom::CreatePushSubscriptionResultCodePtr
                       service_result_code) {
                  std::move(callback).Run(std::move(service_result_code),
                                          requested_settings);
                },
                requested_settings, std::move(subscription),
                std::move(callback), std::move(service_result_code));
            step_1_run_loop.Quit();
          }));
  EXPECT_CALL(mock_callbacks_, DoOnDeviceLaunched(_)).Times(0);
  EXPECT_CALL(mock_callbacks_, OnDeviceLaunchAborted()).Times(1);
  EXPECT_CALL(mock_callbacks_, OnDeviceLaunchFailed(_)).Times(0);
  EXPECT_CALL(connection_lost_cb_, Run()).Times(0);
  EXPECT_CALL(done_cb_, Run()).WillOnce(InvokeWithoutArgs([&step_2_run_loop]() {
    step_2_run_loop.Quit();
  }));

  // Exercise
  launcher_->LaunchDeviceAsync(
      kStubDeviceId, blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
      kArbitraryParams, kNullReceiver, connection_lost_cb_.Get(),
      &mock_callbacks_, done_cb_.Get(), {});
  step_1_run_loop.Run();
  launcher_->AbortLaunch();

  std::move(create_push_subscription_success_answer_cb).Run();
  step_2_run_loop.Run();

  service_connection_.reset();

  EXPECT_TRUE(launcher_has_connected_to_source_provider_);
  EXPECT_TRUE(launcher_has_released_source_provider_);
}

TEST_F(ServiceVideoCaptureDeviceLauncherTest,
       LaunchingDeviceFailsBecauseDeviceNotFound) {
  base::RunLoop run_loop;

  EXPECT_CALL(mock_source_, CreatePushSubscription(_, _, _, _, _))
      .WillOnce(Invoke(
          [](mojo::PendingRemote<video_capture::mojom::VideoFrameHandler>
                 subscriber,
             const media::VideoCaptureParams& requested_settings,
             bool force_reopen_with_new_settings,
             mojo::PendingReceiver<
                 video_capture::mojom::PushVideoStreamSubscription>
                 subscription,
             video_capture::mojom::VideoSource::CreatePushSubscriptionCallback
                 callback) {
            // Note: We post this to the end of the message queue to make it
            // asynchronous.
            base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(
                    [](mojo::PendingRemote<
                           video_capture::mojom::VideoFrameHandler> subscriber,
                       const media::VideoCaptureParams& requested_settings,
                       mojo::PendingReceiver<
                           video_capture::mojom::PushVideoStreamSubscription>
                           subscription,
                       video_capture::mojom::VideoSource::
                           CreatePushSubscriptionCallback callback) {
                      std::move(callback).Run(
                          video_capture::mojom::
                              CreatePushSubscriptionResultCode::NewErrorCode(
                                  media::VideoCaptureError::
                                      kIntentionalErrorRaisedByUnitTest),
                          requested_settings);
                    },
                    std::move(subscriber), requested_settings,
                    std::move(subscription), std::move(callback)));
          }));
  EXPECT_CALL(mock_callbacks_, DoOnDeviceLaunched(_)).Times(0);
  EXPECT_CALL(mock_callbacks_, OnDeviceLaunchAborted()).Times(0);
  EXPECT_CALL(mock_callbacks_,
              OnDeviceLaunchFailed(
                  media::VideoCaptureError::kIntentionalErrorRaisedByUnitTest))
      .Times(1);
  EXPECT_CALL(connection_lost_cb_, Run()).Times(0);
  EXPECT_CALL(done_cb_, Run()).WillOnce(InvokeWithoutArgs([&run_loop]() {
    run_loop.Quit();
  }));

  // Exercise
  launcher_->LaunchDeviceAsync(
      kStubDeviceId, blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
      kArbitraryParams, kNullReceiver, connection_lost_cb_.Get(),
      &mock_callbacks_, done_cb_.Get(), {});
  run_loop.Run();
}

TEST_F(ServiceVideoCaptureDeviceLauncherTest,
       LaunchingDeviceFailsBecauseSourceProviderIsUnbound) {
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);

  EXPECT_CALL(mock_callbacks_, DoOnDeviceLaunched(_)).Times(0);
  EXPECT_CALL(mock_callbacks_, OnDeviceLaunchAborted()).Times(0);
  EXPECT_CALL(mock_callbacks_, OnDeviceLaunchFailed(_)).Times(1);
  EXPECT_CALL(connection_lost_cb_, Run()).Times(0);
  EXPECT_CALL(done_cb_, Run()).WillOnce(InvokeWithoutArgs([&run_loop]() {
    run_loop.Quit();
  }));

  // Exercise
  service_connection_->ReleaseProviderForTesting();

  launcher_->LaunchDeviceAsync(
      kStubDeviceId, blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
      kArbitraryParams, kNullReceiver, connection_lost_cb_.Get(),
      &mock_callbacks_, done_cb_.Get(), {});

  run_loop.Run();
}

TEST_F(ServiceVideoCaptureDeviceLauncherTest,
       LaunchingDeviceFailsBecauseConnectionLostWhileLaunching) {
  base::RunLoop run_loop;

  video_capture::mojom::VideoSource::CreatePushSubscriptionCallback
      create_subscription_cb;
  EXPECT_CALL(mock_source_, CreatePushSubscription(_, _, _, _, _))
      .WillOnce(Invoke(
          [&create_subscription_cb](
              mojo::PendingRemote<video_capture::mojom::VideoFrameHandler>
                  subscriber,
              const media::VideoCaptureParams& requested_settings,
              bool force_reopen_with_new_settings,
              mojo::PendingReceiver<
                  video_capture::mojom::PushVideoStreamSubscription>
                  subscription,
              video_capture::mojom::VideoSource::CreatePushSubscriptionCallback
                  callback) {
            // Simulate connection lost by not invoking |callback| and releasing
            // |subscription|. We have to save |callback| and invoke it later
            // to avoid hitting a DCHECK.
            create_subscription_cb = std::move(callback);
          }));
  EXPECT_CALL(mock_callbacks_, DoOnDeviceLaunched(_)).Times(0);
  EXPECT_CALL(mock_callbacks_, OnDeviceLaunchAborted()).Times(0);
  EXPECT_CALL(mock_callbacks_, OnDeviceLaunchFailed(_)).Times(1);
  // Note: |connection_lost_cb_| is only meant to be called when the connection
  // to a successfully-launched device is lost, which is not the case here.
  EXPECT_CALL(connection_lost_cb_, Run()).Times(0);
  EXPECT_CALL(done_cb_, Run()).WillOnce(InvokeWithoutArgs([&run_loop]() {
    run_loop.Quit();
  }));

  // Exercise
  launcher_->LaunchDeviceAsync(
      kStubDeviceId, blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
      kArbitraryParams, kNullReceiver, connection_lost_cb_.Get(),
      &mock_callbacks_, done_cb_.Get(), {});

  run_loop.Run();

  // Cleanup
  // Cut the connection to the source, so that the outstanding
  // |create_subscription_cb| will be dropped when we invoke it below.
  source_receiver_.reset();
  // We have to invoke the callback, because not doing so triggers a DCHECK.
  std::move(create_subscription_cb)
      .Run(video_capture::mojom::CreatePushSubscriptionResultCode::
               NewSuccessCode(
                   video_capture::mojom::CreatePushSubscriptionSuccessCode::
                       kCreatedWithRequestedSettings),
           kArbitraryParams);
}

TEST_F(ServiceVideoCaptureDeviceLauncherTest,
       ConnectionToSubscriptionLostAfterSuccessfulLaunch) {
  RunConnectionLostAfterSuccessfulStartTest(base::BindOnce(
      &ServiceVideoCaptureDeviceLauncherTest::CloseSourceReceiver,
      base::Unretained(this)));
}

TEST_F(ServiceVideoCaptureDeviceLauncherTest,
       ConnectionToSourceLostAfterSuccessfulLaunch) {
  RunConnectionLostAfterSuccessfulStartTest(base::BindOnce(
      &ServiceVideoCaptureDeviceLauncherTest::CloseSubscriptionReceivers,
      base::Unretained(this)));
}

void ServiceVideoCaptureDeviceLauncherTest::
    RunConnectionLostAfterSuccessfulStartTest(
        base::OnceClosure close_connection_cb) {
  std::unique_ptr<LaunchedVideoCaptureDevice> launched_device;
  EXPECT_CALL(mock_callbacks_, DoOnDeviceLaunched(_))
      .WillOnce(
          Invoke([&launched_device](
                     std::unique_ptr<LaunchedVideoCaptureDevice>* device) {
            // We must keep the launched device alive, because otherwise it will
            // no longer listen for connection errors.
            launched_device = std::move(*device);
          }));
  base::RunLoop step_1_run_loop;
  EXPECT_CALL(done_cb_, Run()).WillOnce(InvokeWithoutArgs([&step_1_run_loop]() {
    step_1_run_loop.Quit();
  }));
  // Exercise step 1
  launcher_->LaunchDeviceAsync(
      kStubDeviceId, blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
      kArbitraryParams, kNullReceiver, connection_lost_cb_.Get(),
      &mock_callbacks_, done_cb_.Get(), {});
  step_1_run_loop.Run();

  base::RunLoop step_2_run_loop;
  EXPECT_CALL(connection_lost_cb_, Run()).WillOnce(Invoke([&step_2_run_loop]() {
    step_2_run_loop.Quit();
  }));
  // Exercise step 2: The service cuts/loses the connection
  std::move(close_connection_cb).Run();
  step_2_run_loop.Run();

  launcher_.reset();
  service_connection_.reset();
  wait_for_release_connection_cb_.Run();
  EXPECT_TRUE(launcher_has_connected_to_source_provider_);
  EXPECT_TRUE(launcher_has_released_source_provider_);
}

}  // namespace content
