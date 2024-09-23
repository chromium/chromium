// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/test/video_capture_service_test.h"

#include "base/command_line.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/media_switches.h"
#include "services/video_capture/public/cpp/mock_producer.h"
#include "services/video_capture/public/mojom/constants.mojom.h"

namespace {
const media::VideoCaptureFormat kDefaultSupportedFormat{
    gfx::Size(640, 480), 30, media::PIXEL_FORMAT_I420};
}  // anonymous namespace

namespace video_capture {

VideoCaptureServiceTest::SharedMemoryVirtualDeviceContext::
    SharedMemoryVirtualDeviceContext(
        mojo::PendingReceiver<mojom::Producer> producer_receiver)
    : mock_producer(
          std::make_unique<MockProducer>(std::move(producer_receiver))) {}

VideoCaptureServiceTest::SharedMemoryVirtualDeviceContext::
    ~SharedMemoryVirtualDeviceContext() = default;

VideoCaptureServiceTest::VideoCaptureServiceTest() = default;

VideoCaptureServiceTest::~VideoCaptureServiceTest() = default;

void VideoCaptureServiceTest::SetUp() {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kUseFakeMjpegDecodeAccelerator);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kUseFakeDeviceForMediaStream, "device-count=3");

  service_impl_ = std::make_unique<VideoCaptureServiceImpl>(
      service_remote_.BindNewPipeAndPassReceiver(),
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      /*create_system_monitor=*/true);

  // Note, that we explicitly do *not* call
  // |service_remote_->InjectGpuDependencies()| here. Test case
  // |FakeMjpegVideoCaptureDeviceTest.
  //  CanDecodeMjpegWithoutInjectedGpuDependencies| depends on this assumption.
  service_remote_->ConnectToVideoSourceProvider(
      video_source_provider_.BindNewPipeAndPassReceiver());

  requestable_settings_.requested_format = kDefaultSupportedFormat;
  requestable_settings_.resolution_change_policy =
      media::ResolutionChangePolicy::FIXED_RESOLUTION;
  requestable_settings_.power_line_frequency =
      media::PowerLineFrequency::kDefault;
}

std::unique_ptr<VideoCaptureServiceTest::SharedMemoryVirtualDeviceContext>
VideoCaptureServiceTest::AddSharedMemoryVirtualDevice(
    const std::string& device_id) {
  media::VideoCaptureDeviceInfo device_info;
  device_info.descriptor.device_id = device_id;
  device_info.supported_formats = {kDefaultSupportedFormat};
  mojo::PendingRemote<mojom::Producer> producer;
  auto result = std::make_unique<SharedMemoryVirtualDeviceContext>(
      producer.InitWithNewPipeAndPassReceiver());
  video_source_provider_->AddSharedMemoryVirtualDevice(
      device_info, std::move(producer),
      result->device.BindNewPipeAndPassReceiver());
  return result;
}

mojo::PendingRemote<mojom::TextureVirtualDevice>
VideoCaptureServiceTest::AddTextureVirtualDevice(const std::string& device_id) {
  media::VideoCaptureDeviceInfo device_info;
  device_info.descriptor.device_id = device_id;
  mojo::PendingRemote<mojom::TextureVirtualDevice> device;
  video_source_provider_->AddTextureVirtualDevice(
      device_info, device.InitWithNewPipeAndPassReceiver());
  return device;
}

}  // namespace video_capture
