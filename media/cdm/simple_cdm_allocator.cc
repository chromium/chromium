// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "media/cdm/simple_cdm_allocator.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/not_fatal_until.h"
#include "media/base/video_frame.h"
#include "media/cdm/cdm_helpers.h"
#include "media/cdm/simple_cdm_buffer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media {

namespace {

class SimpleCdmVideoFrame final : public VideoFrameImpl {
 public:
  SimpleCdmVideoFrame() = default;

  SimpleCdmVideoFrame(const SimpleCdmVideoFrame&) = delete;
  SimpleCdmVideoFrame& operator=(const SimpleCdmVideoFrame&) = delete;

  ~SimpleCdmVideoFrame() override = default;

  // VideoFrameImpl implementation.
  scoped_refptr<media::VideoFrame> TransformToVideoFrame(
      gfx::Size natural_size) override {
    CHECK(FrameBuffer());

    cdm::Buffer* buffer = FrameBuffer();
    // SAFETY: cdm::Buffer is like `span` from CDM stable interface.
    auto buffer_span =
        UNSAFE_BUFFERS(base::span(buffer->Data(), buffer->Size()));
    gfx::Size frame_size(Size().width, Size().height);
    scoped_refptr<media::VideoFrame> frame =
        media::VideoFrame::WrapExternalYuvData(
            PIXEL_FORMAT_I420, frame_size, gfx::Rect(frame_size), natural_size,
            Stride(cdm::kYPlane), Stride(cdm::kUPlane), Stride(cdm::kVPlane),
            buffer_span.subspan(
                PlaneOffset(cdm::kYPlane),
                PlaneOffset(cdm::kUPlane) - PlaneOffset(cdm::kYPlane)),
            buffer_span.subspan(
                PlaneOffset(cdm::kUPlane),
                PlaneOffset(cdm::kVPlane) - PlaneOffset(cdm::kUPlane)),
            buffer_span.subspan(PlaneOffset(cdm::kVPlane)),
            base::Microseconds(Timestamp()));

    frame->metadata().power_efficient = false;

    // TODO(b/183748013): Set HDRMetadata once supported by the CDM interface.
    frame->set_color_space(MediaColorSpace().ToGfxColorSpace());

    // The FrameBuffer needs to remain around until |frame| is destroyed.
    frame->AddDestructionObserver(
        base::BindOnce(&cdm::Buffer::Destroy, base::Unretained(buffer)));

    // Clear FrameBuffer so that SimpleCdmVideoFrame no longer has a reference
    // to it.
    SetFrameBuffer(nullptr);
    return frame;
  }
};

}  // namespace

SimpleCdmAllocator::SimpleCdmAllocator() = default;

SimpleCdmAllocator::~SimpleCdmAllocator() = default;

// Creates a new SimpleCdmBuffer on every request. It does not keep track of
// the memory allocated, so the caller is responsible for calling Destroy()
// on the buffer when it is no longer needed.
cdm::Buffer* SimpleCdmAllocator::CreateCdmBuffer(size_t capacity) {
  if (!capacity)
    return nullptr;

  return SimpleCdmBuffer::Create(capacity);
}

// Creates a new SimpleCdmVideoFrame on every request.
std::unique_ptr<VideoFrameImpl> SimpleCdmAllocator::CreateCdmVideoFrame() {
  return std::make_unique<SimpleCdmVideoFrame>();
}

}  // namespace media
