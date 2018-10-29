// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/test/device_factory_provider_test.h"

#include "base/command_line.h"
#include "media/base/media_switches.h"
#include "services/service_manager/public/mojom/constants.mojom.h"
#include "services/service_manager/public/mojom/service_manager.mojom.h"
#include "services/video_capture/public/cpp/mock_producer.h"
#include "services/video_capture/public/mojom/constants.mojom.h"

namespace video_capture {

DeviceFactoryProviderTest::SharedMemoryVirtualDeviceContext::
    SharedMemoryVirtualDeviceContext(mojom::ProducerRequest producer_request)
    : mock_producer(
          std::make_unique<MockProducer>(std::move(producer_request))) {}

DeviceFactoryProviderTest::SharedMemoryVirtualDeviceContext::
    ~SharedMemoryVirtualDeviceContext() = default;

DeviceFactoryProviderTest::DeviceFactoryProviderTest()
    : service_manager::test::ServiceTest("video_capture_unittests") {}

DeviceFactoryProviderTest::~DeviceFactoryProviderTest() = default;

void DeviceFactoryProviderTest::SetUp() {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kUseFakeJpegDecodeAccelerator);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kUseFakeDeviceForMediaStream, "device-count=3");

  service_manager::test::ServiceTest::SetUp();

  connector()->BindInterface(mojom::kServiceName, &factory_provider_);
  // Note, that we explicitly do *not* call
  // |factory_provider_->InjectGpuDependencies()| here. Test case
  // |FakeMjpegVideoCaptureDeviceTest.
  //  CanDecodeMjpegWithoutInjectedGpuDependencies| depends on this assumption.
  factory_provider_->ConnectToDeviceFactory(mojo::MakeRequest(&factory_));
}

std::unique_ptr<DeviceFactoryProviderTest::SharedMemoryVirtualDeviceContext>
DeviceFactoryProviderTest::AddSharedMemoryVirtualDevice(
    const std::string& device_id) {
  media::VideoCaptureDeviceInfo device_info;
  device_info.descriptor.device_id = device_id;
  mojom::ProducerPtr producer;
  auto result = std::make_unique<SharedMemoryVirtualDeviceContext>(
      mojo::MakeRequest(&producer));
  factory_->AddSharedMemoryVirtualDevice(
      device_info, std::move(producer),
      false /* send_buffer_handles_to_producer_as_raw_file_descriptors */,
      mojo::MakeRequest(&result->device));
  return result;
}

mojom::TextureVirtualDevicePtr
DeviceFactoryProviderTest::AddTextureVirtualDevice(
    const std::string& device_id) {
  media::VideoCaptureDeviceInfo device_info;
  device_info.descriptor.device_id = device_id;
  mojom::TextureVirtualDevicePtr device;
  factory_->AddTextureVirtualDevice(device_info, mojo::MakeRequest(&device));
  return device;
}

}  // namespace video_capture
