// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/device_notifier.h"

#include <memory>
#include <utility>

#include "base/system/system_monitor.h"
#include "base/test/scoped_task_environment.h"
#include "services/audio/public/mojom/device_notifications.mojom.h"
#include "services/audio/traced_service_ref.h"
#include "services/service_manager/public/cpp/service_context_ref.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace audio {

namespace {

class MockDeviceListener : public mojom::DeviceListener {
 public:
  explicit MockDeviceListener(audio::mojom::DeviceListenerRequest request)
      : binding_(this, std::move(request)) {}
  MOCK_METHOD0(DevicesChanged, void());

 private:
  mojo::Binding<audio::mojom::DeviceListener> binding_;

  DISALLOW_COPY_AND_ASSIGN(MockDeviceListener);
};

}  // namespace

class DeviceNotifierTest : public ::testing::Test {
 public:
  DeviceNotifierTest()
      : system_monitor_(std::make_unique<base::SystemMonitor>()),
        service_ref_factory_(
            base::BindRepeating(&DeviceNotifierTest::OnNoServiceRefs,
                                base::Unretained(this))) {}

 protected:
  MOCK_METHOD0(OnNoServiceRefs, void());

  void CreateDeviceNotifier() {
    device_notifier_ = std::make_unique<DeviceNotifier>();
    device_notifier_->Bind(mojo::MakeRequest(&device_notifier_ptr_),
                           TracedServiceRef(service_ref_factory_.CreateRef(),
                                            "audio::DeviceNotifier Binding"));
    EXPECT_FALSE(service_ref_factory_.HasNoRefs());
  }

  void DestroyDeviceNotifier() {
    device_notifier_ptr_.reset();
    scoped_task_environment_.RunUntilIdle();
    EXPECT_TRUE(service_ref_factory_.HasNoRefs());
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  mojom::DeviceNotifierPtr device_notifier_ptr_;

 private:
  std::unique_ptr<base::SystemMonitor> system_monitor_;
  std::unique_ptr<DeviceNotifier> device_notifier_;
  service_manager::ServiceContextRefFactory service_ref_factory_;

  DISALLOW_COPY_AND_ASSIGN(DeviceNotifierTest);
};

TEST_F(DeviceNotifierTest, DeviceNotifierNotifies) {
  EXPECT_CALL(*this, OnNoServiceRefs());
  CreateDeviceNotifier();

  mojom::DeviceListenerPtr device_listener_ptr;
  MockDeviceListener listener(mojo::MakeRequest(&device_listener_ptr));

  // Simulate audio-device event, but no callback should be invoked before the
  // listener is registered.
  EXPECT_CALL(listener, DevicesChanged()).Times(0);
  base::SystemMonitor::Get()->ProcessDevicesChanged(
      base::SystemMonitor::DEVTYPE_AUDIO);
  scoped_task_environment_.RunUntilIdle();

  // Register the listener and simulate an audio-device event.
  device_notifier_ptr_->RegisterListener(std::move(device_listener_ptr));
  EXPECT_CALL(listener, DevicesChanged());
  base::SystemMonitor::Get()->ProcessDevicesChanged(
      base::SystemMonitor::DEVTYPE_AUDIO);
  scoped_task_environment_.RunUntilIdle();

  // Simulate a video-device event, which should be ignored.
  EXPECT_CALL(listener, DevicesChanged()).Times(0);
  base::SystemMonitor::Get()->ProcessDevicesChanged(
      base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);
  scoped_task_environment_.RunUntilIdle();

  DestroyDeviceNotifier();
}

}  // namespace audio
