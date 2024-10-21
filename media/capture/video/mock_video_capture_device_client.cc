// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/mock_video_capture_device_client.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "media/base/video_frame.h"

using testing::_;
using testing::Invoke;
using testing::WithArgs;

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
  const raw_ptr<uint8_t> data_;
};

class StubBufferHandleProvider
    : public VideoCaptureDevice::Client::Buffer::HandleProvider {
 public:
  StubBufferHandleProvider(size_t mapped_size, std::unique_ptr<uint8_t[]> data)
      : mapped_size_(mapped_size), data_(std::move(data)) {}

  ~StubBufferHandleProvider() override = default;

  base::UnsafeSharedMemoryRegion DuplicateAsUnsafeRegion() override {
    return base::UnsafeSharedMemoryRegion();
  }

  std::unique_ptr<VideoCaptureBufferHandle> GetHandleForInProcessAccess()
      override {
    return std::make_unique<StubBufferHandle>(mapped_size_, data_.get());
  }

  gfx::GpuMemoryBufferHandle GetGpuMemoryBufferHandle() override {
    return gfx::GpuMemoryBufferHandle();
  }

 private:
  const size_t mapped_size_;
  const std::unique_ptr<uint8_t[]> data_;
};

class StubReadWritePermission
    : public VideoCaptureDevice::Client::Buffer::ScopedAccessPermission {
 public:
  StubReadWritePermission(uint8_t* data) : data_(data) {}
  ~StubReadWritePermission() override = default;

 private:
  const raw_ptr<uint8_t> data_;
};

VideoCaptureDevice::Client::Buffer CreateStubBuffer(int buffer_id,
                                                    size_t mapped_size) {
  const int arbitrary_frame_feedback_id = 0;
  auto buffer = std::make_unique<uint8_t[]>(mapped_size);
  auto* unowned_buffer = buffer.get();
  return VideoCaptureDevice::Client::Buffer(
      buffer_id, arbitrary_frame_feedback_id,
      std::make_unique<StubBufferHandleProvider>(mapped_size,
                                                 std::move(buffer)),
      std::make_unique<StubReadWritePermission>(unowned_buffer));
}

}  // namespace

MockVideoCaptureDeviceClient::MockVideoCaptureDeviceClient() = default;
MockVideoCaptureDeviceClient::~MockVideoCaptureDeviceClient() = default;

void MockVideoCaptureDeviceClient::OnIncomingCapturedBuffer(
    Buffer buffer,
    const media::VideoCaptureFormat& format,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp,
    std::optional<base::TimeTicks> capture_begin_time) {
  DoOnIncomingCapturedBuffer(buffer, format, reference_time, timestamp);
}
void MockVideoCaptureDeviceClient::OnIncomingCapturedBufferExt(
    Buffer buffer,
    const media::VideoCaptureFormat& format,
    const gfx::ColorSpace& color_space,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp,
    std::optional<base::TimeTicks> capture_begin_time,
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
  ON_CALL(*result, ReserveOutputBuffer)
      .WillByDefault(
          Invoke([](const gfx::Size& dimensions, VideoPixelFormat format, int,
                    VideoCaptureDevice::Client::Buffer* buffer,
                    int* require_new_buffer_id, int* retire_old_buffer_id) {
            EXPECT_GT(dimensions.GetArea(), 0);
            const VideoCaptureFormat frame_format(dimensions, 0.0, format);
            *buffer = CreateStubBuffer(
                0, VideoFrame::AllocationSize(frame_format.pixel_format,
                                              frame_format.frame_size));
            return VideoCaptureDevice::Client::ReserveResult::kSucceeded;
          }));
  ON_CALL(*result, OnIncomingCapturedData)
      .WillByDefault(WithArgs<2>(Invoke(
          [raw_result_ptr](const media::VideoCaptureFormat& frame_format) {
            raw_result_ptr->fake_frame_captured_callback_.Run(frame_format);
          })));
  ON_CALL(*result, OnIncomingCapturedGfxBuffer)
      .WillByDefault(WithArgs<1>(Invoke(
          [raw_result_ptr](const media::VideoCaptureFormat& frame_format) {
            raw_result_ptr->fake_frame_captured_callback_.Run(frame_format);
          })));
  ON_CALL(*result, DoOnIncomingCapturedBuffer)
      .WillByDefault(WithArgs<1>(Invoke(
          [raw_result_ptr](const media::VideoCaptureFormat& frame_format) {
            raw_result_ptr->fake_frame_captured_callback_.Run(frame_format);
          })));
  ON_CALL(*result, DoOnIncomingCapturedBufferExt)
      .WillByDefault(WithArgs<1>(Invoke(
          [raw_result_ptr](const media::VideoCaptureFormat& frame_format) {
            raw_result_ptr->fake_frame_captured_callback_.Run(frame_format);
          })));
  return result;
}

}  // namespace media
