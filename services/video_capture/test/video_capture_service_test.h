// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_TEST_VIDEO_CAPTURE_SERVICE_TEST_H_
#define SERVICES_VIDEO_CAPTURE_TEST_VIDEO_CAPTURE_SERVICE_TEST_H_

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"
#include "services/video_capture/public/mojom/video_source.mojom.h"
#include "services/video_capture/public/mojom/video_source_provider.mojom.h"
#include "services/video_capture/public/mojom/virtual_device.mojom.h"
#include "services/video_capture/video_capture_service_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace video_capture {

class MockProducer;

// Basic test fixture that sets up a connection to the fake device factory.
class VideoCaptureServiceTest : public testing::Test {
 public:
  VideoCaptureServiceTest();

  VideoCaptureServiceTest(const VideoCaptureServiceTest&) = delete;
  VideoCaptureServiceTest& operator=(const VideoCaptureServiceTest&) = delete;

  ~VideoCaptureServiceTest() override;

  void SetUp() override;

 protected:
  struct SharedMemoryVirtualDeviceContext {
    SharedMemoryVirtualDeviceContext(
        mojo::PendingReceiver<mojom::Producer> producer_receiver);
    ~SharedMemoryVirtualDeviceContext();

    std::unique_ptr<MockProducer> mock_producer;
    mojo::Remote<mojom::SharedMemoryVirtualDevice> device;
  };

  std::unique_ptr<SharedMemoryVirtualDeviceContext>
  AddSharedMemoryVirtualDevice(const std::string& device_id);

  mojo::PendingRemote<mojom::TextureVirtualDevice> AddTextureVirtualDevice(
      const std::string& device_id);

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<VideoCaptureServiceImpl> service_impl_;
  mojo::Remote<mojom::VideoCaptureService> service_remote_;
  mojo::Remote<mojom::VideoSourceProvider> video_source_provider_;
  base::MockCallback<mojom::VideoSourceProvider::GetSourceInfosCallback>
      device_info_receiver_;

  base::test::ScopedFeatureList scoped_feature_list_;
  media::VideoCaptureParams requestable_settings_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_TEST_VIDEO_CAPTURE_SERVICE_TEST_H_
