// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/fake_video_capture_stack.h"

#include <stdint.h>

#include <utility>

#include "base/bind_helpers.h"
#include "media/base/video_frame.h"
#include "media/capture/video/video_frame_receiver.h"
#include "media/capture/video_capture_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

FakeVideoCaptureStack::FakeVideoCaptureStack() = default;

FakeVideoCaptureStack::~FakeVideoCaptureStack() = default;

void FakeVideoCaptureStack::Reset() {
  frames_.clear();
  last_frame_timestamp_ = base::TimeDelta::Min();
}

class FakeVideoCaptureStack::Receiver : public media::VideoFrameReceiver {
 public:
  explicit Receiver(FakeVideoCaptureStack* capture_stack)
      : capture_stack_(capture_stack) {}
  ~Receiver() final = default;

 private:
  using Buffer = media::VideoCaptureDevice::Client::Buffer;

  void OnNewBuffer(int buffer_id,
                   media::mojom::VideoBufferHandlePtr buffer_handle) final {
    buffers_[buffer_id] = std::move(buffer_handle);
  }

  void OnFrameReadyInBuffer(
      int buffer_id,
      int frame_feedback_id,
      std::unique_ptr<Buffer::ScopedAccessPermission> access,
      media::mojom::VideoFrameInfoPtr frame_info) final {
    const auto it = buffers_.find(buffer_id);
    CHECK(it != buffers_.end());

    CHECK(it->second->is_read_only_shmem_region());
    base::ReadOnlySharedMemoryMapping mapping =
        it->second->get_read_only_shmem_region().Map();
    CHECK(mapping.IsValid());
    CHECK_LE(media::VideoCaptureFormat(frame_info->coded_size, 0.0f,
                                       frame_info->pixel_format)
                 .ImageAllocationSize(),
             mapping.size());

    auto frame = media::VideoFrame::WrapExternalData(
        frame_info->pixel_format, frame_info->coded_size,
        frame_info->visible_rect, frame_info->visible_rect.size(),
        const_cast<uint8_t*>(static_cast<const uint8_t*>(mapping.memory())),
        mapping.size(), frame_info->timestamp);
    CHECK(frame);
    frame->metadata()->MergeInternalValuesFrom(frame_info->metadata);
    if (frame_info->color_space.has_value())
      frame->set_color_space(frame_info->color_space.value());
    // This destruction observer will unmap the shared memory when the
    // VideoFrame goes out-of-scope.
    frame->AddDestructionObserver(base::BindOnce(
        base::DoNothing::Once<base::ReadOnlySharedMemoryMapping>(),
        std::move(mapping)));
    // This destruction observer will notify the video capture device once all
    // downstream code is done using the VideoFrame.
    frame->AddDestructionObserver(base::BindOnce(
        [](std::unique_ptr<Buffer::ScopedAccessPermission> access) {},
        std::move(access)));

    capture_stack_->OnReceivedFrame(std::move(frame));
  }

  void OnBufferRetired(int buffer_id) final {
    const auto it = buffers_.find(buffer_id);
    CHECK(it != buffers_.end());
    buffers_.erase(it);
  }

  void OnError(media::VideoCaptureError) final {
    capture_stack_->error_occurred_ = true;
  }

  void OnFrameDropped(media::VideoCaptureFrameDropReason) final {}

  void OnLog(const std::string& message) final {
    capture_stack_->log_messages_.push_back(message);
  }

  void OnStarted() final { capture_stack_->started_ = true; }

  void OnStartedUsingGpuDecode() final { NOTREACHED(); }

  FakeVideoCaptureStack* const capture_stack_;
  base::flat_map<int, media::mojom::VideoBufferHandlePtr> buffers_;

  DISALLOW_COPY_AND_ASSIGN(Receiver);
};

std::unique_ptr<media::VideoFrameReceiver>
FakeVideoCaptureStack::CreateFrameReceiver() {
  return std::make_unique<Receiver>(this);
}

SkBitmap FakeVideoCaptureStack::NextCapturedFrame() {
  CHECK(!frames_.empty());
  media::VideoFrame& frame = *(frames_.front());
  SkBitmap bitmap;
  bitmap.allocN32Pixels(frame.visible_rect().width(),
                        frame.visible_rect().height());
  // TODO(crbug/810131): This is not Rec.709 colorspace conversion, and so will
  // introduce inaccuracies.
  libyuv::I420ToARGB(frame.visible_data(media::VideoFrame::kYPlane),
                     frame.stride(media::VideoFrame::kYPlane),
                     frame.visible_data(media::VideoFrame::kUPlane),
                     frame.stride(media::VideoFrame::kUPlane),
                     frame.visible_data(media::VideoFrame::kVPlane),
                     frame.stride(media::VideoFrame::kVPlane),
                     reinterpret_cast<uint8_t*>(bitmap.getPixels()),
                     static_cast<int>(bitmap.rowBytes()), bitmap.width(),
                     bitmap.height());
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

  frames_.emplace_back(std::move(frame));
}

}  // namespace content
