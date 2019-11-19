// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/video_frame_utils.h"

#include "base/bind_helpers.h"
#include "media/video/fake_gpu_memory_buffer.h"

namespace blink {

scoped_refptr<media::VideoFrame> CreateTestFrame(
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    media::VideoFrame::StorageType storage_type) {
  scoped_refptr<media::VideoFrame> frame;
  switch (storage_type) {
    case media::VideoFrame::STORAGE_OWNED_MEMORY:
      frame = media::VideoFrame::CreateFrame(media::PIXEL_FORMAT_I420,
                                             coded_size, visible_rect,
                                             natural_size, base::TimeDelta());
      break;

    case media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER: {
      auto gmb = std::make_unique<media::FakeGpuMemoryBuffer>(
          coded_size, gfx::BufferFormat::YUV_420_BIPLANAR);
      const gpu::MailboxHolder empty_mailboxes[media::VideoFrame::kMaxPlanes];
      frame = media::VideoFrame::WrapExternalGpuMemoryBuffer(
          visible_rect, natural_size, std::move(gmb), empty_mailboxes,
          base::NullCallback(), base::TimeDelta());
      break;
    }

    default:
      NOTREACHED() << "Unexpected storage type";
  }
  return frame;
}

}  // namespace blink
