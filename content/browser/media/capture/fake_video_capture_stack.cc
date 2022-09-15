// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/fake_video_capture_stack.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "content/browser/media/capture/frame_test_util.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/base/video_util.h"
#include "media/capture/video/video_frame_receiver.h"
#include "media/capture/video_capture_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace content {

FakeVideoCaptureStack::FakeVideoCaptureStack() = default;

FakeVideoCaptureStack::~FakeVideoCaptureStack() = default;

void FakeVideoCaptureStack::Reset() {
  frames_.clear();
  last_frame_timestamp_ = base::TimeDelta::Min();
}

class FakeVideoCaptureStack::Receiver final : public media::VideoFrameReceiver {
 public:
  explicit Receiver(FakeVideoCaptureStack* capture_stack)
      : capture_stack_(capture_stack) {}

  Receiver(const Receiver&) = delete;
  Receiver& operator=(const Receiver&) = delete;

  ~Receiver() override = default;

 private:
  using Buffer = media::VideoCaptureDevice::Client::Buffer;

  void OnNewBuffer(int buffer_id,
                   media::mojom::VideoBufferHandlePtr buffer_handle) override {
    buffers_[buffer_id] = std::move(buffer_handle);
  }

  scoped_refptr<media::VideoFrame> GetVideoFrameFromSharedMemory(
      media::ReadyFrameInBuffer frame,
      base::ReadOnlySharedMemoryMapping mapping) {
    CHECK(mapping.IsValid());

    const auto& frame_format = media::VideoCaptureFormat(
        frame.frame_info->coded_size, 0.0f, frame.frame_info->pixel_format);
    CHECK_LE(media::VideoFrame::AllocationSize(frame_format.pixel_format,
                                               frame_format.frame_size),
             mapping.size());

    auto video_frame = media::VideoFrame::WrapExternalData(
        frame.frame_info->pixel_format, frame.frame_info->coded_size,
        frame.frame_info->visible_rect, frame.frame_info->visible_rect.size(),
        const_cast<uint8_t*>(static_cast<const uint8_t*>(mapping.memory())),
        mapping.size(), frame.frame_info->timestamp);
    CHECK(video_frame);

    video_frame->set_metadata(frame.frame_info->metadata);
    if (frame.frame_info->color_space.has_value()) {
      video_frame->set_color_space(frame.frame_info->color_space.value());
    }

    // This destruction observer will unmap the shared memory when the
    // VideoFrame goes out-of-scope.
    video_frame->AddDestructionObserver(base::BindOnce(
        [](base::ReadOnlySharedMemoryMapping) {}, std::move(mapping)));
    // This destruction observer will notify the video capture device once all
    // downstream code is done using the VideoFrame.
    video_frame->AddDestructionObserver(base::BindOnce(
        [](std::unique_ptr<Buffer::ScopedAccessPermission> access) {},
        std::move(frame.buffer_read_permission)));

    return video_frame;
  }

  scoped_refptr<media::VideoFrame> GetVideoFrameFromGpuMemoryBuffer(
      media::ReadyFrameInBuffer frame,
      const gfx::GpuMemoryBufferHandle& gmb_handle) {
    CHECK(!gmb_handle.is_null());
    CHECK_EQ(frame.frame_info->pixel_format,
             media::VideoPixelFormat::PIXEL_FORMAT_NV12);

    gpu::GpuMemoryBufferSupport gmb_support;
    std::unique_ptr<gfx::GpuMemoryBuffer> gmb =
        gmb_support.CreateGpuMemoryBufferImplFromHandle(
            gmb_handle.Clone(), frame.frame_info->coded_size,
            gfx::BufferFormat::YUV_420_BIPLANAR,
            gfx::BufferUsage::SCANOUT_VEA_CPU_READ, base::DoNothing());
    CHECK(gmb);

    gfx::Size size = gmb->GetSize();
    gpu::MailboxHolder mailbox_holders[media::VideoFrame::kMaxPlanes];
    auto video_frame = media::VideoFrame::WrapExternalGpuMemoryBuffer(
        frame.frame_info->visible_rect, size, std::move(gmb), mailbox_holders,
        base::BindOnce([](const gpu::SyncToken& token,
                          std::unique_ptr<gfx::GpuMemoryBuffer> gmb) {}),
        frame.frame_info->timestamp);
    CHECK(video_frame);

    video_frame->set_metadata(frame.frame_info->metadata);
    if (frame.frame_info->color_space.has_value()) {
      video_frame->set_color_space(frame.frame_info->color_space.value());
    }

    auto mapped_frame = media::ConvertToMemoryMappedFrame(video_frame);
    CHECK(mapped_frame);

    // This destruction observer will notify the video capture device once all
    // downstream code is done using the VideoFrame.
    mapped_frame->AddDestructionObserver(base::BindOnce(
        [](std::unique_ptr<Buffer::ScopedAccessPermission> access) {},
        std::move(frame.buffer_read_permission)));

    return mapped_frame;
  }

  void OnFrameReadyInBuffer(
      media::ReadyFrameInBuffer frame,
      std::vector<media::ReadyFrameInBuffer> scaled_frames) override {
    const auto it = buffers_.find(frame.buffer_id);
    CHECK(it != buffers_.end());

    CHECK(it->second->is_read_only_shmem_region() ||
          it->second->is_gpu_memory_buffer_handle());

    scoped_refptr<media::VideoFrame> video_frame = nullptr;
    if (it->second->is_read_only_shmem_region()) {
      video_frame = GetVideoFrameFromSharedMemory(
          std::move(frame), it->second->get_read_only_shmem_region().Map());
    } else {
      video_frame = GetVideoFrameFromGpuMemoryBuffer(
          std::move(frame), it->second->get_gpu_memory_buffer_handle());
    }

    // This implementation does not forward scaled frames.
    capture_stack_->OnReceivedFrame(std::move(video_frame));
  }

  void OnBufferRetired(int buffer_id) override {
    const auto it = buffers_.find(buffer_id);
    CHECK(it != buffers_.end());
    buffers_.erase(it);
  }

  void OnError(media::VideoCaptureError) override {
    capture_stack_->error_occurred_ = true;
  }

  void OnFrameDropped(media::VideoCaptureFrameDropReason) override {}

  void OnNewCropVersion(uint32_t crop_version) override {}

  void OnFrameWithEmptyRegionCapture() override {}

  void OnLog(const std::string& message) override {
    capture_stack_->log_messages_.push_back(message);
  }

  void OnStarted() override { capture_stack_->started_ = true; }

  void OnStartedUsingGpuDecode() override { NOTREACHED(); }

  void OnStopped() override {}

  const raw_ptr<FakeVideoCaptureStack> capture_stack_;
  base::flat_map<int, media::mojom::VideoBufferHandlePtr> buffers_;
};

std::unique_ptr<media::VideoFrameReceiver>
FakeVideoCaptureStack::CreateFrameReceiver() {
  return std::make_unique<Receiver>(this);
}

SkBitmap FakeVideoCaptureStack::NextCapturedFrame() {
  CHECK(!frames_.empty());
  media::VideoFrame& frame = *(frames_.front());
  SkBitmap bitmap = FrameTestUtil::ConvertToBitmap(frame);
  frames_.pop_front();
  return bitmap;
}

void FakeVideoCaptureStack::ClearCapturedFramesQueue() {
  frames_.clear();
}

void FakeVideoCaptureStack::ExpectHasLogMessages() {
  EXPECT_FALSE(log_messages_.empty());
  while (!log_messages_.empty()) {
    VLOG(1) << "Next log message: " << log_messages_.front();
    log_messages_.pop_front();
  }
}

void FakeVideoCaptureStack::ExpectNoLogMessages() {
  while (!log_messages_.empty()) {
    ADD_FAILURE() << "Unexpected log message: " << log_messages_.front();
    log_messages_.pop_front();
  }
}

void FakeVideoCaptureStack::OnReceivedFrame(
    scoped_refptr<media::VideoFrame> frame) {
  // Frame timestamps should be monotionically increasing.
  EXPECT_LT(last_frame_timestamp_, frame->timestamp());
  last_frame_timestamp_ = frame->timestamp();

  EXPECT_TRUE(frame->ColorSpace().IsValid());

  if (on_frame_received_) {
    on_frame_received_.Run(frame.get());
  }

  frames_.emplace_back(std::move(frame));
}

}  // namespace content
