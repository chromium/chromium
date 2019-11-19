// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/device_notifier.h"

#include <memory>
#include <utility>

#include "base/system/system_monitor.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/audio/public/mojom/device_notifications.mojom.h"
#include "services/audio/traced_service_ref.h"
#include "services/service_manager/public/cpp/service_keepalive.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace audio {

namespace {

class MockDeviceListener : public mojom::DeviceListener {
 public:
  explicit MockDeviceListener(
      mojo::PendingReceiver<audio::mojom::DeviceListener> receiver)
      : receiver_(this, std::move(receiver)) {}
  MOCK_METHOD0(DevicesChanged, void());

 private:
  mojo::Receiver<audio::mojom::DeviceListener> receiver_;

  DISALLOW_COPY_AND_ASSIGN(MockDeviceListener);
};

}  // namespace

class DeviceNotifierTest : public ::testing::Test,
                           public service_manager::ServiceKeepalive::Observer {
 public:
  DeviceNotifierTest()
      : system_monitor_(std::make_unique<base::SystemMonitor>()),
        service_keepalive_(nullptr, base::TimeDelta()) {
    service_keepalive_.AddObserver(this);
  }

 protected:
  MOCK_METHOD0(OnNoServiceRefs, void());

  void CreateDeviceNotifier() {
    device_notifier_ = std::make_unique<DeviceNotifier>();
    device_notifier_->Bind(remote_device_notifier_.BindNewPipeAndPassReceiver(),
                           TracedServiceRef(service_keepalive_.CreateRef(),
                                            "audio::DeviceNotifier Binding"));
    EXPECT_FALSE(service_keepalive_.HasNoRefs());
  }

  void DestroyDeviceNotifier() {
    remote_device_notifier_.reset();
    task_environment_.RunUntilIdle();
    EXPECT_TRUE(service_keepalive_.HasNoRefs());
  }

  // service_manager::ServiceKeepalive::Observer:
  void OnIdleTimeout() override { OnNoServiceRefs(); }

  base::test::TaskEnvironment task_environment_;
  mojo::Remote<mojom::DeviceNotifier> remote_device_notifier_;

 private:
  std::unique_ptr<base::SystemMonitor> system_monitor_;
  std::unique_ptr<DeviceNotifier> device_notifier_;
  service_manager::ServiceKeepalive service_keepalive_;

  DISALLOW_COPY_AND_ASSIGN(DeviceNotifierTest);
};

TEST_F(DeviceNotifierTest, DeviceNotifierNotifies) {
  EXPECT_CALL(*this, OnNoServiceRefs());
  CreateDeviceNotifier();

  mojo::PendingRemote<mojom::DeviceListener> remote_device_listener;
  MockDeviceListener listener(
      remote_device_listener.InitWithNewPipeAndPassReceiver());

  // Simulate audio-device event, but no callback should be invoked before the
  // listener is registered.
  EXPECT_CALL(listener, DevicesChanged()).Times(0);
  base::SystemMonitor::Get()->ProcessDevicesChanged(
      base::SystemMonitor::DEVTYPE_AUDIO);
  task_environment_.RunUntilIdle();

  // Register the listener and simulate an audio-device event.
  remote_device_notifier_->RegisterListener(std::move(remote_device_listener));
  EXPECT_CALL(listener, DevicesChanged());
  base::SystemMonitor::Get()->ProcessDevicesChanged(
      base::SystemMonitor::DEVTYPE_AUDIO);
  task_environment_.RunUntilIdle();

  // Simulate a video-device event, which should be ignored.
  EXPECT_CALL(listener, DevicesChanged()).Times(0);
  base::SystemMonitor::Get()->ProcessDevicesChanged(
      base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);
  task_environment_.RunUntilIdle();

  DestroyDeviceNotifier();
}

}  // namespace audio
