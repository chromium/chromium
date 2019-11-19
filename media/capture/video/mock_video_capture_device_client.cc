// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/mock_video_capture_device_client.h"

using testing::_;
using testing::Invoke;

namespace media {

namespace {

class StubBufferHandle : public VideoCaptureBufferHandle {
 public:
  StubBufferHandle(size_t mapped_size, uint8_t* data)
      : mapped_size_(mapped_size), data_(data) {}

  size_t mapped_size() const override { return mapped_size_; }
  uint8_t* data() const override { return data_; }
  const uint8_t* const_data() const override { return data_; }

 private:
  const size_t mapped_size_;
  uint8_t* const data_;
};

class StubBufferHandleProvider
    : public VideoCaptureDevice::Client::Buffer::HandleProvider {
 public:
  StubBufferHandleProvider(size_t mapped_size, uint8_t* data)
      : mapped_size_(mapped_size), data_(data) {}

  ~StubBufferHandleProvider() override = default;

  base::UnsafeSharedMemoryRegion DuplicateAsUnsafeRegion() override {
    NOTREACHED();
    return {};
  }

  mojo::ScopedSharedBufferHandle DuplicateAsMojoBuffer() override {
    NOTREACHED();
    return mojo::ScopedSharedBufferHandle();
  }

  std::unique_ptr<VideoCaptureBufferHandle> GetHandleForInProcessAccess()
      override {
    return std::make_unique<StubBufferHandle>(mapped_size_, data_);
  }

  gfx::GpuMemoryBufferHandle GetGpuMemoryBufferHandle() override {
    return gfx::GpuMemoryBufferHandle();
  }

 private:
  const size_t mapped_size_;
  uint8_t* const data_;
};

class StubReadWritePermission
    : public VideoCaptureDevice::Client::Buffer::ScopedAccessPermission {
 public:
  StubReadWritePermission(uint8_t* data) : data_(data) {}
  ~StubReadWritePermission() override { delete[] data_; }

 private:
  uint8_t* const data_;
};

VideoCaptureDevice::Client::Buffer CreateStubBuffer(int buffer_id,
                                                    size_t mapped_size) {
  auto* buffer = new uint8_t[mapped_size];
  const int arbitrary_frame_feedback_id = 0;
  return VideoCaptureDevice::Client::Buffer(
      buffer_id, arbitrary_frame_feedback_id,
      std::make_unique<StubBufferHandleProvider>(mapped_size, buffer),
      std::make_unique<StubReadWritePermission>(buffer));
}

}  // namespace

MockVideoCaptureDeviceClient::MockVideoCaptureDeviceClient() = default;
MockVideoCaptureDeviceClient::~MockVideoCaptureDeviceClient() = default;

void MockVideoCaptureDeviceClient::OnIncomingCapturedBuffer(
    Buffer buffer,
    const media::VideoCaptureFormat& format,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp) {
  DoOnIncomingCapturedBuffer(buffer, format, reference_time, timestamp);
}
void MockVideoCaptureDeviceClient::OnIncomingCapturedBufferExt(
    Buffer buffer,
    const media::VideoCaptureFormat& format,
    const gfx::ColorSpace& color_space,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp,
    gfx::Rect visible_rect,
    const media::VideoFrameMetadata& additional_metadata) {
  DoOnIncomingCapturedBufferExt(buffer, format, color_space, reference_time,
                                timestamp, visible_rect, additional_metadata);
}

// static
std::unique_ptr<MockVideoCaptureDeviceClient>
MockVideoCaptureDeviceClient::CreateMockClientWithBufferAllocator(
    FakeFrameCapturedCallback frame_captured_callback) {
  auto result = std::make_unique<NiceMockVideoCaptureDeviceClient>();
  result->fake_frame_captured_callback_ = std::move(frame_captured_callback);

  auto* raw_result_ptr = result.get();
  ON_CALL(*result, ReserveOutputBuffer(_, _, _, _))
      .WillByDefault(
          Invoke([](const gfx::Size& dimensions, VideoPixelFormat format, int,
                    VideoCaptureDevice::Client::Buffer* buffer) {
            EXPECT_GT(dimensions.GetArea(), 0);
            const VideoCaptureFormat frame_format(dimensions, 0.0, format);
            *buffer = CreateStubBuffer(0, frame_format.ImageAllocationSize());
            return VideoCaptureDevice::Client::ReserveResult::kSucceeded;
          }));
  ON_CALL(*result, OnIncomingCapturedData(_, _, _, _, _, _, _, _, _))
      .WillByDefault(
          Invoke([raw_result_ptr](const uint8_t*, int,
                                  const media::VideoCaptureFormat& frame_format,
                                  const gfx::ColorSpace&, int, bool,
                                  base::TimeTicks, base::TimeDelta, int) {
            raw_result_ptr->fake_frame_captured_callback_.Run(frame_format);
          }));
  ON_CALL(*result, OnIncomingCapturedGfxBuffer(_, _, _, _, _, _))
      .WillByDefault(
          Invoke([raw_result_ptr](gfx::GpuMemoryBuffer*,
                                  const media::VideoCaptureFormat& frame_format,
                                  int, base::TimeTicks, base::TimeDelta, int) {
            raw_result_ptr->fake_frame_captured_callback_.Run(frame_format);
          }));
  ON_CALL(*result, DoOnIncomingCapturedBuffer(_, _, _, _))
      .WillByDefault(
          Invoke([raw_result_ptr](media::VideoCaptureDevice::Client::Buffer&,
                                  const media::VideoCaptureFormat& frame_format,
                                  base::TimeTicks, base::TimeDelta) {
            raw_result_ptr->fake_frame_captured_callback_.Run(frame_format);
          }));
  ON_CALL(*result, DoOnIncomingCapturedBufferExt(_, _, _, _, _, _, _))
      .WillByDefault(
          Invoke([raw_result_ptr](media::VideoCaptureDevice::Client::Buffer&,
                                  const media::VideoCaptureFormat& frame_format,
                                  const gfx::ColorSpace&, base::TimeTicks,
                                  base::TimeDelta, gfx::Rect,
                                  const media::VideoFrameMetadata&) {
            raw_result_ptr->fake_frame_captured_callback_.Run(frame_format);
          }));
  return result;
}

}  // namespace media
