// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/test/fake_device_descriptor_test.h"

#include "base/run_loop.h"

using testing::_;
using testing::Invoke;

namespace video_capture {

FakeDeviceDescriptorTest::FakeDeviceDescriptorTest()
    : VideoCaptureServiceTest() {}

FakeDeviceDescriptorTest::~FakeDeviceDescriptorTest() = default;

void FakeDeviceDescriptorTest::SetUp() {
  VideoCaptureServiceTest::SetUp();

  base::RunLoop wait_loop;
  EXPECT_CALL(device_info_receiver_, Run)
      .WillOnce(Invoke(
          [this, &wait_loop](
              video_capture::mojom::VideoSourceProvider::GetSourceInfosResult,
              const std::vector<media::VideoCaptureDeviceInfo>& infos) {
            CHECK(infos.size() >= 3);
            i420_fake_device_info_ = infos[0];
            mjpeg_fake_device_info_ = infos[2];
            wait_loop.Quit();
          }));
  video_source_provider_->GetSourceInfos(device_info_receiver_.Get());
  wait_loop.Run();
}

}  // namespace video_capture
