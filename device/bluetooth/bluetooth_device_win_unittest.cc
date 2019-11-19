// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_device_win.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/test_simple_task_runner.h"
#include "device/bluetooth/bluetooth_service_record_win.h"
#include "device/bluetooth/bluetooth_socket_thread.h"
#include "device/bluetooth/bluetooth_task_manager_win.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kDeviceName[] = "Device";
const char kDeviceAddress[] = "01:02:03:0A:10:A0";

const char kTestAudioSdpName[] = "Audio";
const char kTestAudioSdpBytes[] =
    "35510900000a00010001090001350319110a09000435103506190100090019350619001909"
    "010209000535031910020900093508350619110d090102090100250c417564696f20536f75"
    "726365090311090001";
const device::BluetoothUUID kTestAudioSdpUuid("110a");

const char kTestVideoSdpName[] = "Video";
const char kTestVideoSdpBytes[] =
    "354b0900000a000100030900013506191112191203090004350c3503190100350519000308"
    "0b090005350319100209000935083506191108090100090100250d566f6963652047617465"
    "776179";
const device::BluetoothUUID kTestVideoSdpUuid("1112");

}  // namespace

namespace device {

class BluetoothDeviceWinTest : public testing::Test {
 public:
  BluetoothDeviceWinTest() {
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner(
        new base::TestSimpleTaskRunner());
    scoped_refptr<BluetoothSocketThread> socket_thread(
        BluetoothSocketThread::Get());

    // Add device with audio/video services.
    device_state_.reset(new BluetoothTaskManagerWin::DeviceState());
    device_state_->name = std::string(kDeviceName);
    device_state_->address = kDeviceAddress;

    auto audio_state =
        std::make_unique<BluetoothTaskManagerWin::ServiceRecordState>();
    audio_state->name = kTestAudioSdpName;
    base::HexStringToBytes(kTestAudioSdpBytes, &audio_state->sdp_bytes);
    device_state_->service_record_states.push_back(std::move(audio_state));

    auto video_state =
        std::make_unique<BluetoothTaskManagerWin::ServiceRecordState>();
    video_state->name = kTestVideoSdpName;
    base::HexStringToBytes(kTestVideoSdpBytes, &video_state->sdp_bytes);
    device_state_->service_record_states.push_back(std::move(video_state));

    device_.reset(new BluetoothDeviceWin(nullptr, *device_state_,
                                         ui_task_runner, socket_thread));

    // Add empty device.
    empty_device_state_.reset(new BluetoothTaskManagerWin::DeviceState());
    empty_device_state_->name = std::string(kDeviceName);
    empty_device_state_->address = kDeviceAddress;
    empty_device_.reset(new BluetoothDeviceWin(nullptr, *empty_device_state_,
                                               ui_task_runner, socket_thread));
  }

 protected:
  std::unique_ptr<BluetoothDeviceWin> device_;
  std::unique_ptr<BluetoothTaskManagerWin::DeviceState> device_state_;
  std::unique_ptr<BluetoothDeviceWin> empty_device_;
  std::unique_ptr<BluetoothTaskManagerWin::DeviceState> empty_device_state_;
};

TEST_F(BluetoothDeviceWinTest, GetUUIDs) {
  BluetoothDevice::UUIDSet uuids = device_->GetUUIDs();

  EXPECT_EQ(2u, uuids.size());
  EXPECT_TRUE(base::Contains(uuids, kTestAudioSdpUuid));
  EXPECT_TRUE(base::Contains(uuids, kTestVideoSdpUuid));

  uuids = empty_device_->GetUUIDs();
  EXPECT_EQ(0u, uuids.size());
}

TEST_F(BluetoothDeviceWinTest, IsEqual) {
  EXPECT_TRUE(device_->IsEqual(*device_state_));
  EXPECT_FALSE(device_->IsEqual(*empty_device_state_));
  EXPECT_FALSE(empty_device_->IsEqual(*device_state_));
  EXPECT_TRUE(empty_device_->IsEqual(*empty_device_state_));
}

}  // namespace device
