// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_TEST_DEVICE_FACTORY_PROVIDER_TEST_H_
#define SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_TEST_DEVICE_FACTORY_PROVIDER_TEST_H_

#include "base/test/mock_callback.h"
#include "services/service_manager/public/cpp/service_test.h"
#include "services/video_capture/public/mojom/device_factory_provider.mojom.h"

namespace video_capture {

class MockProducer;

// Basic test fixture that sets up a connection to the fake device factory.
class DeviceFactoryProviderTest : public service_manager::test::ServiceTest {
 public:
  DeviceFactoryProviderTest();
  ~DeviceFactoryProviderTest() override;

  void SetUp() override;

 protected:
  struct SharedMemoryVirtualDeviceContext {
    SharedMemoryVirtualDeviceContext(mojom::ProducerRequest producer_request);
    ~SharedMemoryVirtualDeviceContext();

    std::unique_ptr<MockProducer> mock_producer;
    mojom::SharedMemoryVirtualDevicePtr device;
  };

  std::unique_ptr<SharedMemoryVirtualDeviceContext>
  AddSharedMemoryVirtualDevice(const std::string& device_id);

  mojom::TextureVirtualDevicePtr AddTextureVirtualDevice(
      const std::string& device_id);

  mojom::DeviceFactoryProviderPtr factory_provider_;
  mojom::DeviceFactoryPtr factory_;
  base::MockCallback<mojom::DeviceFactory::GetDeviceInfosCallback>
      device_info_receiver_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_TEST_DEVICE_FACTORY_PROVIDER_TEST_H_
