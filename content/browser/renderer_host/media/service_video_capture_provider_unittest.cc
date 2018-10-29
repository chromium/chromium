// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/service_video_capture_provider.h"

#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/threading/thread.h"
#include "content/public/browser/video_capture_device_launcher.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/video_capture/public/cpp/mock_device_factory.h"
#include "services/video_capture/public/cpp/mock_device_factory_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Mock;
using testing::Invoke;
using testing::_;

namespace content {

static const std::string kStubDeviceId = "StubDevice";
static const media::VideoCaptureParams kArbitraryParams;
static const base::WeakPtr<media::VideoFrameReceiver> kNullReceiver;
static const auto kIgnoreLogMessageCB =
    base::BindRepeating([](const std::string&) {});

class MockServiceConnector
    : public ServiceVideoCaptureProvider::ServiceConnector {
 public:
  MockServiceConnector() {}

  MOCK_METHOD1(BindFactoryProvider,
               void(video_capture::mojom::DeviceFactoryProviderPtr* provider));
};

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

class ServiceVideoCaptureProviderTest : public testing::Test {
 public:
  ServiceVideoCaptureProviderTest()
      : factory_provider_binding_(&mock_device_factory_provider_),
        device_factory_binding_(&mock_device_factory_) {}
  ~ServiceVideoCaptureProviderTest() override {}

 protected:
  void SetUp() override {
    auto mock_service_connector = std::make_unique<MockServiceConnector>();
    mock_service_connector_ = mock_service_connector.get();
    provider_ = std::make_unique<ServiceVideoCaptureProvider>(
        std::move(mock_service_connector), base::BindRepeating([]() {
          return std::unique_ptr<video_capture::mojom::AcceleratorFactory>();
        }),
        kIgnoreLogMessageCB);

    ON_CALL(*mock_service_connector_, BindFactoryProvider(_))
        .WillByDefault(
            Invoke([this](video_capture::mojom::DeviceFactoryProviderPtr*
                              device_factory_provider) {
              if (factory_provider_binding_.is_bound())
                factory_provider_binding_.Close();
              factory_provider_binding_.Bind(
                  mojo::MakeRequest(device_factory_provider));
            }));
    ON_CALL(mock_device_factory_provider_, DoConnectToDeviceFactory(_))
        .WillByDefault(
            Invoke([this](video_capture::mojom::DeviceFactoryRequest& request) {
              if (device_factory_binding_.is_bound())
                device_factory_binding_.Close();
              device_factory_binding_.Bind(std::move(request));
              wait_for_connection_to_service_.Quit();
            }));
  }

  void TearDown() override {}

  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  MockServiceConnector* mock_service_connector_;
  video_capture::MockDeviceFactoryProvider mock_device_factory_provider_;
  mojo::Binding<video_capture::mojom::DeviceFactoryProvider>
      factory_provider_binding_;
  video_capture::MockDeviceFactory mock_device_factory_;
  mojo::Binding<video_capture::mojom::DeviceFactory> device_factory_binding_;
  std::unique_ptr<ServiceVideoCaptureProvider> provider_;
  base::MockCallback<VideoCaptureProvider::GetDeviceInfosCallback> results_cb_;
  base::MockCallback<
      video_capture::mojom::DeviceFactory::GetDeviceInfosCallback>
      service_cb_;
  base::RunLoop wait_for_connection_to_service_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceVideoCaptureProviderTest);
};

// Tests that if connection to the service is lost during an outstanding call
// to GetDeviceInfos(), the callback passed into GetDeviceInfos() still gets
// invoked.
TEST_F(ServiceVideoCaptureProviderTest,
       GetDeviceInfosAsyncInvokesCallbackWhenLosingConnection) {
  base::RunLoop run_loop;

  video_capture::mojom::DeviceFactory::GetDeviceInfosCallback
      callback_to_be_called_by_service;
  base::RunLoop wait_for_call_to_arrive_at_service;
  EXPECT_CALL(mock_device_factory_, DoGetDeviceInfos(_))
      .WillOnce(Invoke(
          [&callback_to_be_called_by_service,
           &wait_for_call_to_arrive_at_service](
              video_capture::mojom::DeviceFactory::GetDeviceInfosCallback&
                  callback) {
            // Hold on to the callback so we can drop it later.
            callback_to_be_called_by_service = std::move(callback);
            wait_for_call_to_arrive_at_service.Quit();
          }));
  base::RunLoop wait_for_callback_from_service;
  EXPECT_CALL(results_cb_, Run(_))
      .WillOnce(Invoke(
          [&wait_for_callback_from_service](
              const std::vector<media::VideoCaptureDeviceInfo>& results) {
            EXPECT_EQ(0u, results.size());
            wait_for_callback_from_service.Quit();
          }));

  // Exercise
  provider_->GetDeviceInfosAsync(results_cb_.Get());
  wait_for_call_to_arrive_at_service.Run();

  // Simulate that the service goes down by cutting the connections.
  device_factory_binding_.Close();
  factory_provider_binding_.Close();

  wait_for_callback_from_service.Run();
}

// Tests that |ServiceVideoCaptureProvider| closes the connection to the service
// after successfully processing a single request to GetDeviceInfos().
TEST_F(ServiceVideoCaptureProviderTest,
       ClosesServiceConnectionAfterGetDeviceInfos) {
  // Setup part 1
  video_capture::mojom::DeviceFactory::GetDeviceInfosCallback
      callback_to_be_called_by_service;
  base::RunLoop wait_for_call_to_arrive_at_service;
  EXPECT_CALL(mock_device_factory_, DoGetDeviceInfos(_))
      .WillOnce(Invoke(
          [&callback_to_be_called_by_service,
           &wait_for_call_to_arrive_at_service](
              video_capture::mojom::DeviceFactory::GetDeviceInfosCallback&
                  callback) {
            // Hold on to the callback so we can drop it later.
            callback_to_be_called_by_service = std::move(callback);
            wait_for_call_to_arrive_at_service.Quit();
          }));

  // Exercise part 1: Make request to the service
  provider_->GetDeviceInfosAsync(results_cb_.Get());
  wait_for_call_to_arrive_at_service.Run();

  // Setup part 2: Now that the connection to the service is established, we can
  // listen for disconnects.
  base::RunLoop wait_for_connection_to_device_factory_to_close;
  base::RunLoop wait_for_connection_to_device_factory_provider_to_close;
  device_factory_binding_.set_connection_error_handler(
      base::BindOnce([](base::RunLoop* run_loop) { run_loop->Quit(); },
                     &wait_for_connection_to_device_factory_to_close));
  factory_provider_binding_.set_connection_error_handler(
      base::BindOnce([](base::RunLoop* run_loop) { run_loop->Quit(); },
                     &wait_for_connection_to_device_factory_provider_to_close));

  // Exercise part 2: The service responds
  std::vector<media::VideoCaptureDeviceInfo> arbitrarily_empty_results;
  base::ResetAndReturn(&callback_to_be_called_by_service)
      .Run(arbitrarily_empty_results);

  // Verification: Expect |provider_| to close the connection to the service.
  wait_for_connection_to_device_factory_to_close.Run();
  wait_for_connection_to_device_factory_provider_to_close.Run();
}

// Tests that |ServiceVideoCaptureProvider| does not close the connection to the
// service while at least one previously handed out VideoCaptureDeviceLauncher
// instance is still using it. Then confirms that it closes the connection as
// soon as the last VideoCaptureDeviceLauncher instance is released.
TEST_F(ServiceVideoCaptureProviderTest,
       KeepsServiceConnectionWhileDeviceLauncherAlive) {
  ON_CALL(mock_device_factory_, DoGetDeviceInfos(_))
      .WillByDefault(Invoke([](video_capture::mojom::DeviceFactory::
                                   GetDeviceInfosCallback& callback) {
        std::vector<media::VideoCaptureDeviceInfo> arbitrarily_empty_results;
        base::ResetAndReturn(&callback).Run(arbitrarily_empty_results);
      }));
  ON_CALL(mock_device_factory_, DoCreateDevice(_, _, _))
      .WillByDefault(
          Invoke([](const std::string& device_id,
                    video_capture::mojom::DeviceRequest* device_request,
                    video_capture::mojom::DeviceFactory::CreateDeviceCallback&
                        callback) {
            base::ResetAndReturn(&callback).Run(
                video_capture::mojom::DeviceAccessResultCode::SUCCESS);
          }));
  MockVideoCaptureDeviceLauncherCallbacks mock_callbacks;

  // Exercise part 1: Create a device launcher and hold on to it.
  auto device_launcher_1 = provider_->CreateDeviceLauncher();
  base::RunLoop wait_for_launch_1;
  device_launcher_1->LaunchDeviceAsync(
      kStubDeviceId, content::MEDIA_DEVICE_VIDEO_CAPTURE, kArbitraryParams,
      kNullReceiver, base::DoNothing(), &mock_callbacks,
      wait_for_launch_1.QuitClosure());
  wait_for_connection_to_service_.Run();
  wait_for_launch_1.Run();

  // Monitor if connection gets closed
  bool connection_has_been_closed = false;
  device_factory_binding_.set_connection_error_handler(base::BindOnce(
      [](bool* connection_has_been_closed) {
        *connection_has_been_closed = true;
      },
      &connection_has_been_closed));
  factory_provider_binding_.set_connection_error_handler(base::BindOnce(
      [](bool* connection_has_been_closed) {
        *connection_has_been_closed = true;
      },
      &connection_has_been_closed));

  // Exercise part 2: Make a few GetDeviceInfosAsync requests
  base::RunLoop wait_for_get_device_infos_response_1;
  base::RunLoop wait_for_get_device_infos_response_2;
  provider_->GetDeviceInfosAsync(base::BindOnce(
      [](base::RunLoop* run_loop,
         const std::vector<media::VideoCaptureDeviceInfo>&) {
        run_loop->Quit();
      },
      &wait_for_get_device_infos_response_1));
  provider_->GetDeviceInfosAsync(base::BindOnce(
      [](base::RunLoop* run_loop,
         const std::vector<media::VideoCaptureDeviceInfo>&) {
        run_loop->Quit();
      },
      &wait_for_get_device_infos_response_2));
  wait_for_get_device_infos_response_1.Run();
  {
    base::RunLoop give_provider_chance_to_disconnect;
    give_provider_chance_to_disconnect.RunUntilIdle();
  }
  ASSERT_FALSE(connection_has_been_closed);
  wait_for_get_device_infos_response_2.Run();
  {
    base::RunLoop give_provider_chance_to_disconnect;
    give_provider_chance_to_disconnect.RunUntilIdle();
  }
  ASSERT_FALSE(connection_has_been_closed);

  // Exercise part 3: Create and release another device launcher
  auto device_launcher_2 = provider_->CreateDeviceLauncher();
  base::RunLoop wait_for_launch_2;
  device_launcher_2->LaunchDeviceAsync(
      kStubDeviceId, content::MEDIA_DEVICE_VIDEO_CAPTURE, kArbitraryParams,
      kNullReceiver, base::DoNothing(), &mock_callbacks,
      wait_for_launch_2.QuitClosure());
  wait_for_launch_2.Run();
  device_launcher_2.reset();
  {
    base::RunLoop give_provider_chance_to_disconnect;
    give_provider_chance_to_disconnect.RunUntilIdle();
  }
  ASSERT_FALSE(connection_has_been_closed);

  // Exercise part 3: Release the initial device launcher.
  device_launcher_1.reset();
  {
    base::RunLoop give_provider_chance_to_disconnect;
    give_provider_chance_to_disconnect.RunUntilIdle();
  }
  ASSERT_TRUE(connection_has_been_closed);
}

// Tests that |ServiceVideoCaptureProvider| does not close the connection to the
// service while at least one answer to a GetDeviceInfos() request is still
// pending. Confirms that it closes the connection as soon as the last pending
// request is answered.
TEST_F(ServiceVideoCaptureProviderTest,
       DoesNotCloseServiceConnectionWhileGetDeviceInfoResponsePending) {
  // When GetDeviceInfos gets called, hold on to the callbacks, but do not
  // yet invoke them.
  std::vector<video_capture::mojom::DeviceFactory::GetDeviceInfosCallback>
      callbacks_to_be_called_by_service;
  ON_CALL(mock_device_factory_, DoGetDeviceInfos(_))
      .WillByDefault(Invoke(
          [&callbacks_to_be_called_by_service](
              video_capture::mojom::DeviceFactory::GetDeviceInfosCallback&
                  callback) {
            callbacks_to_be_called_by_service.push_back(std::move(callback));
          }));

  // Make initial call to GetDeviceInfosAsync(). The service does not yet
  // respond.
  provider_->GetDeviceInfosAsync(
      base::BindOnce([](const std::vector<media::VideoCaptureDeviceInfo>&) {}));
  // Make an additional call to GetDeviceInfosAsync().
  provider_->GetDeviceInfosAsync(
      base::BindOnce([](const std::vector<media::VideoCaptureDeviceInfo>&) {}));
  {
    base::RunLoop give_mojo_chance_to_process;
    give_mojo_chance_to_process.RunUntilIdle();
  }
  ASSERT_EQ(2u, callbacks_to_be_called_by_service.size());

  // Monitor if connection gets closed
  bool connection_has_been_closed = false;
  device_factory_binding_.set_connection_error_handler(base::BindOnce(
      [](bool* connection_has_been_closed) {
        *connection_has_been_closed = true;
      },
      &connection_has_been_closed));
  factory_provider_binding_.set_connection_error_handler(base::BindOnce(
      [](bool* connection_has_been_closed) {
        *connection_has_been_closed = true;
      },
      &connection_has_been_closed));

  // The service now responds to the first request.
  std::vector<media::VideoCaptureDeviceInfo> arbitrarily_empty_results;
  base::ResetAndReturn(&callbacks_to_be_called_by_service[0])
      .Run(arbitrarily_empty_results);
  {
    base::RunLoop give_provider_chance_to_disconnect;
    give_provider_chance_to_disconnect.RunUntilIdle();
  }
  ASSERT_FALSE(connection_has_been_closed);

  // Create and release a device launcher
  auto device_launcher = provider_->CreateDeviceLauncher();
  device_launcher.reset();
  {
    base::RunLoop give_provider_chance_to_disconnect;
    give_provider_chance_to_disconnect.RunUntilIdle();
  }
  ASSERT_FALSE(connection_has_been_closed);

  // The service now responds to the second request.
  base::ResetAndReturn(&callbacks_to_be_called_by_service[1])
      .Run(arbitrarily_empty_results);
  {
    base::RunLoop give_provider_chance_to_disconnect;
    give_provider_chance_to_disconnect.RunUntilIdle();
  }
  ASSERT_TRUE(connection_has_been_closed);
}

}  // namespace content
