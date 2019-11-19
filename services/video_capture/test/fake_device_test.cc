// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/test/fake_device_test.h"

#include "base/bind.h"
#include "base/run_loop.h"

using testing::_;
using testing::Invoke;

namespace video_capture {

FakeDeviceTest::FakeDeviceTest() : FakeDeviceDescriptorTest() {}

FakeDeviceTest::~FakeDeviceTest() = default;

void FakeDeviceTest::SetUp() {
  FakeDeviceDescriptorTest::SetUp();

  ASSERT_LE(1u, i420_fake_device_info_.supported_formats.size());
  fake_device_first_supported_format_ =
      i420_fake_device_info_.supported_formats[0];

  requestable_settings_.requested_format = fake_device_first_supported_format_;
  requestable_settings_.resolution_change_policy =
      media::ResolutionChangePolicy::FIXED_RESOLUTION;
  requestable_settings_.power_line_frequency =
      media::PowerLineFrequency::FREQUENCY_DEFAULT;

  factory_->CreateDevice(
      std::move(i420_fake_device_info_.descriptor.device_id),
      i420_fake_device_remote_.BindNewPipeAndPassReceiver(),
      base::BindOnce([](mojom::DeviceAccessResultCode result_code) {
        ASSERT_EQ(mojom::DeviceAccessResultCode::SUCCESS, result_code);
      }));

  factory_->CreateDevice(
      std::move(mjpeg_fake_device_info_.descriptor.device_id),
      mjpeg_fake_device_remote_.BindNewPipeAndPassReceiver(),
      base::BindOnce([](mojom::DeviceAccessResultCode result_code) {
        ASSERT_EQ(mojom::DeviceAccessResultCode::SUCCESS, result_code);
      }));
}

}  // namespace video_capture
