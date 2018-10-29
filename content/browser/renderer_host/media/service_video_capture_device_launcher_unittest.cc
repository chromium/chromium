// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/service_video_capture_device_launcher.h"

#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/threading/thread.h"
#include "content/browser/renderer_host/media/service_launched_video_capture_device.h"
#include "content/browser/renderer_host/media/video_capture_factory_delegate.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/video_capture/public/cpp/mock_device_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::_;

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
  ~ServiceVideoCaptureDeviceLauncherTest() override {}

 protected:
  void SetUp() override {
    factory_binding_ =
        std::make_unique<mojo::Binding<video_capture::mojom::DeviceFactory>>(
            &mock_device_factory_, mojo::MakeRequest(&device_factory_));
    launcher_ = std::make_unique<ServiceVideoCaptureDeviceLauncher>(
        connect_to_device_factory_cb_.Get());
    launcher_has_connected_to_device_factory_ = false;
    launcher_has_released_device_factory_ = false;

    ON_CALL(connect_to_device_factory_cb_, Run(_))
        .WillByDefault(Invoke(
            [this](std::unique_ptr<VideoCaptureFactoryDelegate>* out_factory) {
              launcher_has_connected_to_device_factory_ = true;
              *out_factory = std::make_unique<VideoCaptureFactoryDelegate>(
                  &device_factory_, release_connection_cb_.Get());
            }));

    ON_CALL(release_connection_cb_, Run())
        .WillByDefault(InvokeWithoutArgs([this]() {
          launcher_has_released_device_factory_ = true;
          wait_for_release_connection_cb_.Quit();
        }));
  }

  void TearDown() override {}

  void RunLaunchingDeviceIsAbortedTest(
      video_capture::mojom::DeviceAccessResultCode service_result_code);

  TestBrowserThreadBundle thread_bundle_;
  video_capture::MockDeviceFactory mock_device_factory_;
  MockVideoCaptureDeviceLauncherCallbacks mock_callbacks_;
  video_capture::mojom::DeviceFactoryPtr device_factory_;
  std::unique_ptr<mojo::Binding<video_capture::mojom::DeviceFactory>>
      factory_binding_;
  std::unique_ptr<ServiceVideoCaptureDeviceLauncher> launcher_;
  base::MockCallback<base::OnceClosure> connection_lost_cb_;
  base::MockCallback<base::OnceClosure> done_cb_;
  base::MockCallback<
      ServiceVideoCaptureDeviceLauncher::ConnectToDeviceFactoryCB>
      connect_to_device_factory_cb_;
  base::MockCallback<base::OnceClosure> release_connection_cb_;
  bool launcher_has_connected_to_device_factory_;
  bool launcher_has_released_device_factory_;
  base::RunLoop wait_for_release_connection_cb_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceVideoCaptureDeviceLauncherTest);
};

TEST_F(ServiceVideoCaptureDeviceLauncherTest, LaunchingDeviceSucceeds) {
  EXPECT_CALL(mock_device_factory_, DoCreateDevice(kStubDeviceId, _, _))
      .WillOnce(Invoke([](const std::string& device_id,
                          video_capture::mojom::DeviceRequest* device_request,
                          video_capture::mojom::DeviceFactory::
                              CreateDeviceCallback& callback) {
        // Note: We must keep |device_request| alive at least until we have
        // sent out the callback. Otherwise, |launcher_| may interpret this
        // as the connection having been lost before receiving the callback.
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE,
            base::BindOnce(
                [](video_capture::mojom::DeviceRequest device_request,
                   video_capture::mojom::DeviceFactory::CreateDeviceCallback
                       callback) {
                  std::move(callback).Run(
                      video_capture::mojom::DeviceAccessResultCode::SUCCESS);
                },
                std::move(*device_request), std::move(callback)));
      }));

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
      kStubDeviceId, content::MEDIA_DEVICE_VIDEO_CAPTURE, kArbitraryParams,
      kNullReceiver, connection_lost_cb_.Get(), &mock_callbacks_,
      done_cb_.Get());
  wait_for_done_cb.Run();

  launcher_.reset();
  wait_for_release_connection_cb_.Run();
  EXPECT_TRUE(launcher_has_connected_to_device_factory_);
  EXPECT_TRUE(launcher_has_released_device_factory_);
}

TEST_F(ServiceVideoCaptureDeviceLauncherTest,
       LaunchingDeviceIsAbortedBeforeServiceRespondsWithSuccess) {
  RunLaunchingDeviceIsAbortedTest(
      video_capture::mojom::DeviceAccessResultCode::SUCCESS);
}

TEST_F(ServiceVideoCaptureDeviceLauncherTest,
       LaunchingDeviceIsAbortedBeforeServiceRespondsWithNotFound) {
  RunLaunchingDeviceIsAbortedTest(
      video_capture::mojom::DeviceAccessResultCode::ERROR_DEVICE_NOT_FOUND);
}

TEST_F(ServiceVideoCaptureDeviceLauncherTest,
       LaunchingDeviceIsAbortedBeforeServiceRespondsWithNotInitialized) {
  RunLaunchingDeviceIsAbortedTest(
      video_capture::mojom::DeviceAccessResultCode::NOT_INITIALIZED);
}

void ServiceVideoCaptureDeviceLauncherTest::RunLaunchingDeviceIsAbortedTest(
    video_capture::mojom::DeviceAccessResultCode service_result_code) {
  base::RunLoop step_1_run_loop;
  base::RunLoop step_2_run_loop;

  base::OnceClosure create_device_success_answer_cb;
  EXPECT_CALL(mock_device_factory_, DoCreateDevice(kStubDeviceId, _, _))
      .WillOnce(
          Invoke([&create_device_success_answer_cb, &step_1_run_loop,
                  service_result_code](
                     const std::string& device_id,
                     video_capture::mojom::DeviceRequest* device_request,
                     video_capture::mojom::DeviceFactory::CreateDeviceCallback&
                         callback) {
            // Prepare the callback, but save it for now instead of invoking it.
            create_device_success_answer_cb = base::BindOnce(
                [](video_capture::mojom::DeviceRequest device_request,
                   video_capture::mojom::DeviceFactory::CreateDeviceCallback
                       callback,
                   video_capture::mojom::DeviceAccessResultCode
                       service_result_code) {
                  std::move(callback).Run(service_result_code);
                },
                std::move(*device_request), std::move(callback),
                service_result_code);
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
      kStubDeviceId, content::MEDIA_DEVICE_VIDEO_CAPTURE, kArbitraryParams,
      kNullReceiver, connection_lost_cb_.Get(), &mock_callbacks_,
      done_cb_.Get());
  step_1_run_loop.Run();
  launcher_->AbortLaunch();

  std::move(create_device_success_answer_cb).Run();
  step_2_run_loop.Run();

  EXPECT_TRUE(launcher_has_connected_to_device_factory_);
  EXPECT_TRUE(launcher_has_released_device_factory_);
}

TEST_F(ServiceVideoCaptureDeviceLauncherTest,
       LaunchingDeviceFailsBecauseDeviceNotFound) {
  base::RunLoop run_loop;

  EXPECT_CALL(mock_device_factory_, DoCreateDevice(kStubDeviceId, _, _))
      .WillOnce(
          Invoke([](const std::string& device_id,
                    video_capture::mojom::DeviceRequest* device_request,
                    video_capture::mojom::DeviceFactory::CreateDeviceCallback&
                        callback) {
            // Note: We must keep |device_request| alive at least until we have
            // sent out the callback. Otherwise, |launcher_| may interpret this
            // as the connection having been lost before receiving the callback.
            base::ThreadTaskRunnerHandle::Get()->PostTask(
                FROM_HERE,
                base::BindOnce(
                    [](video_capture::mojom::DeviceRequest device_request,
                       video_capture::mojom::DeviceFactory::CreateDeviceCallback
                           callback) {
                      std::move(callback).Run(
                          video_capture::mojom::DeviceAccessResultCode::
                              ERROR_DEVICE_NOT_FOUND);
                    },
                    std::move(*device_request), std::move(callback)));
          }));
  EXPECT_CALL(mock_callbacks_, DoOnDeviceLaunched(_)).Times(0);
  EXPECT_CALL(mock_callbacks_, OnDeviceLaunchAborted()).Times(0);
  EXPECT_CALL(mock_callbacks_,
              OnDeviceLaunchFailed(
                  media::VideoCaptureError::
                      kServiceDeviceLauncherServiceRespondedWithDeviceNotFound))
      .Times(1);
  EXPECT_CALL(connection_lost_cb_, Run()).Times(0);
  EXPECT_CALL(done_cb_, Run()).WillOnce(InvokeWithoutArgs([&run_loop]() {
    run_loop.Quit();
  }));

  // Exercise
  launcher_->LaunchDeviceAsync(
      kStubDeviceId, content::MEDIA_DEVICE_VIDEO_CAPTURE, kArbitraryParams,
      kNullReceiver, connection_lost_cb_.Get(), &mock_callbacks_,
      done_cb_.Get());
  run_loop.Run();
}

TEST_F(ServiceVideoCaptureDeviceLauncherTest,
       LaunchingDeviceFailsBecauseFactoryIsUnbound) {
  base::RunLoop run_loop;

  EXPECT_CALL(mock_callbacks_, DoOnDeviceLaunched(_)).Times(0);
  EXPECT_CALL(mock_callbacks_, OnDeviceLaunchAborted()).Times(0);
  EXPECT_CALL(mock_callbacks_, OnDeviceLaunchFailed(_)).Times(1);
  EXPECT_CALL(connection_lost_cb_, Run()).Times(0);
  EXPECT_CALL(done_cb_, Run()).WillOnce(InvokeWithoutArgs([&run_loop]() {
    run_loop.Quit();
  }));

  // Exercise
  device_factory_.reset();
  launcher_->LaunchDeviceAsync(
      kStubDeviceId, content::MEDIA_DEVICE_VIDEO_CAPTURE, kArbitraryParams,
      kNullReceiver, connection_lost_cb_.Get(), &mock_callbacks_,
      done_cb_.Get());

  run_loop.Run();
}

TEST_F(ServiceVideoCaptureDeviceLauncherTest,
       LaunchingDeviceFailsBecauseConnectionLostWhileLaunching) {
  base::RunLoop run_loop;

  video_capture::mojom::DeviceFactory::CreateDeviceCallback create_device_cb;
  EXPECT_CALL(mock_device_factory_, DoCreateDevice(kStubDeviceId, _, _))
      .WillOnce(
          Invoke([&create_device_cb](
                     const std::string& device_id,
                     video_capture::mojom::DeviceRequest* device_request,
                     video_capture::mojom::DeviceFactory::CreateDeviceCallback&
                         callback) {
            // Simulate connection lost by not invoking |callback| and releasing
            // |device_request|. We have to save |callback| and invoke it later
            // to avoid hitting a DCHECK.
            create_device_cb = std::move(callback);
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
      kStubDeviceId, content::MEDIA_DEVICE_VIDEO_CAPTURE, kArbitraryParams,
      kNullReceiver, connection_lost_cb_.Get(), &mock_callbacks_,
      done_cb_.Get());

  run_loop.Run();

  // Cut the connection to the factory, so that the outstanding
  // |create_device_cb| will be dropped.
  factory_binding_.reset();
  // We have to invoke the callback, because not doing so triggers a DCHECK.
  const video_capture::mojom::DeviceAccessResultCode arbitrary_result_code =
      video_capture::mojom::DeviceAccessResultCode::SUCCESS;
  std::move(create_device_cb).Run(arbitrary_result_code);
}

TEST_F(ServiceVideoCaptureDeviceLauncherTest,
       ConnectionLostAfterSuccessfulLaunch) {
  video_capture::mojom::DeviceRequest device_request_owned_by_service;
  EXPECT_CALL(mock_device_factory_, DoCreateDevice(kStubDeviceId, _, _))
      .WillOnce(Invoke([&device_request_owned_by_service](
                           const std::string& device_id,
                           video_capture::mojom::DeviceRequest* device_request,
                           video_capture::mojom::DeviceFactory::
                               CreateDeviceCallback& callback) {
        // The service holds on to the |device_request|.
        device_request_owned_by_service = std::move(*device_request);
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE,
            base::BindOnce(
                [](video_capture::mojom::DeviceFactory::CreateDeviceCallback
                       callback) {
                  std::move(callback).Run(
                      video_capture::mojom::DeviceAccessResultCode::SUCCESS);
                },
                std::move(callback)));
      }));
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
      kStubDeviceId, content::MEDIA_DEVICE_VIDEO_CAPTURE, kArbitraryParams,
      kNullReceiver, connection_lost_cb_.Get(), &mock_callbacks_,
      done_cb_.Get());
  step_1_run_loop.Run();

  base::RunLoop step_2_run_loop;
  EXPECT_CALL(connection_lost_cb_, Run()).WillOnce(Invoke([&step_2_run_loop]() {
    step_2_run_loop.Quit();
  }));
  // Exercise step 2: The service cuts/loses the connection
  device_request_owned_by_service = nullptr;
  step_2_run_loop.Run();

  launcher_.reset();
  wait_for_release_connection_cb_.Run();
  EXPECT_TRUE(launcher_has_connected_to_device_factory_);
  EXPECT_TRUE(launcher_has_released_device_factory_);
}

}  // namespace content
