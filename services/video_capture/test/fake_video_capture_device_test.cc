// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/test/fake_video_capture_device_test.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"

using testing::_;
using testing::Invoke;

namespace video_capture {

FakeVideoCaptureDeviceTest::FakeVideoCaptureDeviceTest()
    : FakeDeviceDescriptorTest() {}

FakeVideoCaptureDeviceTest::~FakeVideoCaptureDeviceTest() = default;

void FakeVideoCaptureDeviceTest::SetUp() {
  FakeDeviceDescriptorTest::SetUp();

  ASSERT_LE(1u, i420_fake_device_info_.supported_formats.size());
  fake_device_first_supported_format_ =
      i420_fake_device_info_.supported_formats[0];

  requestable_settings_.requested_format = fake_device_first_supported_format_;
  requestable_settings_.resolution_change_policy =
      media::ResolutionChangePolicy::FIXED_RESOLUTION;
  requestable_settings_.power_line_frequency =
      media::PowerLineFrequency::kDefault;

  video_source_provider_->GetVideoSource(
      i420_fake_device_info_.descriptor.device_id,
      i420_fake_source_remote_.BindNewPipeAndPassReceiver());

  video_source_provider_->GetVideoSource(
      mjpeg_fake_device_info_.descriptor.device_id,
      mjpeg_fake_source_remote_.BindNewPipeAndPassReceiver());
}
}  // namespace video_capture
