// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/test/video_capture_service_test.h"

#include "base/command_line.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/media_switches.h"
#include "services/video_capture/public/cpp/mock_producer.h"
#include "services/video_capture/public/mojom/constants.mojom.h"

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
      base::ThreadTaskRunnerHandle::Get());

  // Note, that we explicitly do *not* call
  // |service_remote_->InjectGpuDependencies()| here. Test case
  // |FakeMjpegVideoCaptureDeviceTest.
  //  CanDecodeMjpegWithoutInjectedGpuDependencies| depends on this assumption.
  service_remote_->ConnectToDeviceFactory(
      factory_.BindNewPipeAndPassReceiver());
}

std::unique_ptr<VideoCaptureServiceTest::SharedMemoryVirtualDeviceContext>
VideoCaptureServiceTest::AddSharedMemoryVirtualDevice(
    const std::string& device_id) {
  media::VideoCaptureDeviceInfo device_info;
  device_info.descriptor.device_id = device_id;
  mojo::PendingRemote<mojom::Producer> producer;
  auto result = std::make_unique<SharedMemoryVirtualDeviceContext>(
      producer.InitWithNewPipeAndPassReceiver());
  factory_->AddSharedMemoryVirtualDevice(
      device_info, std::move(producer),
      false /* send_buffer_handles_to_producer_as_raw_file_descriptors */,
      result->device.BindNewPipeAndPassReceiver());
  return result;
}

mojo::PendingRemote<mojom::TextureVirtualDevice>
VideoCaptureServiceTest::AddTextureVirtualDevice(const std::string& device_id) {
  media::VideoCaptureDeviceInfo device_info;
  device_info.descriptor.device_id = device_id;
  mojo::PendingRemote<mojom::TextureVirtualDevice> device;
  factory_->AddTextureVirtualDevice(device_info,
                                    device.InitWithNewPipeAndPassReceiver());
  return device;
}

}  // namespace video_capture
