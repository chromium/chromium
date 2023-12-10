// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/mock_devices_changed_observer.h"
#include "content/browser/renderer_host/media/media_devices_manager.h"
#include "content/public/browser/video_capture_service.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/public/cpp/mock_video_capture_service.h"
#include "services/video_capture/public/cpp/mock_video_source_provider.h"
#include "services/video_capture/public/mojom/video_source_provider.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;

namespace content {

class MockVideoCaptureDevicesChangedObserver
    : public MediaDevicesManager::VideoCaptureDevicesChangedObserver {
 public:
  MockVideoCaptureDevicesChangedObserver()
      : VideoCaptureDevicesChangedObserver(
            /*disconnect_cb=*/base::BindRepeating(
                &MockVideoCaptureDevicesChangedObserver::HandleDevicesChanged,
                base::Unretained(this),
                MediaDeviceType::kMediaVideoInput),
            /*listener_cb=*/base::BindRepeating([] {
              if (auto* monitor = base::SystemMonitor::Get()) {
                monitor->ProcessDevicesChanged(
                    base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);
              }
            })),
        source_provider_receiver_(&mock_source_provider_) {
    OverrideVideoCaptureServiceForTesting(&mock_video_capture_service_);
    ON_CALL(mock_video_capture_service_, DoConnectToVideoSourceProvider(_))
        .WillByDefault(Invoke(
            [this](
                mojo::PendingReceiver<video_capture::mojom::VideoSourceProvider>
                    receiver) {
              if (source_provider_receiver_.is_bound()) {
                source_provider_receiver_.reset();
              }
              source_provider_receiver_.Bind(std::move(receiver));
              base::RunLoop().RunUntilIdle();
            }));

    ConnectToService();
    base::RunLoop().RunUntilIdle();
  }

  void CrashService() { source_provider_receiver_.reset(); }

  MOCK_METHOD1(HandleDevicesChanged, void(MediaDeviceType type));

  mojo::Receiver<video_capture::mojom::VideoSourceProvider>
      source_provider_receiver_;
  video_capture::MockVideoCaptureService mock_video_capture_service_;
  video_capture::MockVideoSourceProvider mock_source_provider_;
};

class DevicesChangedObserverTest : public testing::Test {
 public:
  DevicesChangedObserverTest()
      : mock_video_capture_service_device_changed_observer_(
            std::make_unique<MockVideoCaptureDevicesChangedObserver>()) {
    system_monitor_.AddDevicesChangedObserver(&observer);
  }

  void RaiseVirtualDeviceChangeEvent() {
    mock_video_capture_service_device_changed_observer_->mock_source_provider_
        .RaiseVirtualDeviceChangeEvent();
  }

  void RaiseDeviceChangeEvent() {
    mock_video_capture_service_device_changed_observer_->mock_source_provider_
        .RaiseDeviceChangeEvent();
  }

  void TearDown() override {
    system_monitor_.RemoveDevicesChangedObserver(&observer);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::SystemMonitor system_monitor_;
  base::MockDevicesChangedObserver observer;
  std::unique_ptr<MockVideoCaptureDevicesChangedObserver>
      mock_video_capture_service_device_changed_observer_;
};

TEST_F(DevicesChangedObserverTest,
       RegisteredObserverReceivesDeviceChangeEvents) {
  EXPECT_CALL(observer,
              OnDevicesChanged(base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE))
      .Times(1);
  RaiseDeviceChangeEvent();
  base::RunLoop().RunUntilIdle();

  // No OnDevicesChanged event should be observed when a virtual device change
  // event occurs.
  RaiseVirtualDeviceChangeEvent();
  base::RunLoop().RunUntilIdle();
}

TEST_F(DevicesChangedObserverTest, ServiceCrashes) {
  EXPECT_CALL(observer,
              OnDevicesChanged(base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE))
      .Times(2);
  RaiseDeviceChangeEvent();
  base::RunLoop().RunUntilIdle();

  // Crashing the service should call disconnect_cb resulting in calling
  // |HandleDevicesChanged|. But device change events are still registered
  // because the OnConnectionError() function calls ConnectToService().
  EXPECT_CALL(*mock_video_capture_service_device_changed_observer_,
              HandleDevicesChanged(MediaDeviceType::kMediaVideoInput))
      .Times(1);
  mock_video_capture_service_device_changed_observer_->CrashService();
  base::RunLoop().RunUntilIdle();
  RaiseDeviceChangeEvent();
  base::RunLoop().RunUntilIdle();
}

}  // namespace content
