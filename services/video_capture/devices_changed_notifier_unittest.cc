// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/devices_changed_notifier.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FakeDevicesChangedObserver
    : public video_capture::mojom::DevicesChangedObserver {
 public:
  explicit FakeDevicesChangedObserver(
      mojo::PendingReceiver<video_capture::mojom::DevicesChangedObserver>
          receiver)
      : receiver_(this, std::move(receiver)) {}
  void OnDevicesChanged() override { devices_changed_call_count_++; }

  size_t devices_changed_call_count() { return devices_changed_call_count_; }

 private:
  size_t devices_changed_call_count_ = 0;
  mojo::Receiver<video_capture::mojom::DevicesChangedObserver> receiver_;
};

}  // namespace

namespace video_capture {

class DevicesChangedNotifierTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  base::SystemMonitor system_monitor_;
  DevicesChangedNotifier devices_changed_notifier_;
};

TEST_F(DevicesChangedNotifierTest, RegisterObserver) {
  mojo::PendingRemote<mojom::DevicesChangedObserver> observer_remote;
  FakeDevicesChangedObserver devices_changed_observer(
      observer_remote.InitWithNewPipeAndPassReceiver());
  devices_changed_notifier_.RegisterObserver(std::move(observer_remote));
  base::RunLoop().RunUntilIdle();

  base::SystemMonitor::Get()->ProcessDevicesChanged(
      base::SystemMonitor::DeviceType::DEVTYPE_VIDEO_CAPTURE);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, devices_changed_observer.devices_changed_call_count());

  base::SystemMonitor::Get()->ProcessDevicesChanged(
      base::SystemMonitor::DeviceType::DEVTYPE_AUDIO);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, devices_changed_observer.devices_changed_call_count());

  base::SystemMonitor::Get()->ProcessDevicesChanged(
      base::SystemMonitor::DeviceType::DEVTYPE_VIDEO_CAPTURE);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, devices_changed_observer.devices_changed_call_count());
}

}  // namespace video_capture
