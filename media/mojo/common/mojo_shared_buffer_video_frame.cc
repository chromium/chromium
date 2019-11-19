// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/common/mojo_shared_buffer_video_frame.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace media {

// static
scoped_refptr<MojoSharedBufferVideoFrame>
MojoSharedBufferVideoFrame::CreateDefaultI420ForTesting(
    const gfx::Size& dimensions,
    base::TimeDelta timestamp) {
  const VideoPixelFormat format = PIXEL_FORMAT_I420;
  const gfx::Rect visible_rect(dimensions);

  // Since we're allocating memory for the new frame, pad the requested
  // size if necessary so that the requested size does line up on sample
  // boundaries. See related discussion in VideoFrame::CreateFrameInternal().
  const gfx::Size coded_size = DetermineAlignedSize(format, dimensions);
  if (!IsValidConfig(format, STORAGE_MOJO_SHARED_BUFFER, coded_size,
                     visible_rect, dimensions)) {
    LOG(DFATAL) << __func__ << " Invalid config. "
                << ConfigToString(format, STORAGE_MOJO_SHARED_BUFFER,
                                  dimensions, visible_rect, dimensions);
    return nullptr;
  }

  // Allocate a shared memory buffer big enough to hold the desired frame.
  const size_t allocation_size = VideoFrame::AllocationSize(format, coded_size);
  mojo::ScopedSharedBufferHandle handle =
      mojo::SharedBufferHandle::Create(allocation_size);
  if (!handle.is_valid()) {
    DLOG(ERROR) << __func__ << " Unable to allocate memory.";
    return nullptr;
  }

  // Create and initialize the frame. As this is I420 format, the U and V
  // planes have samples for each 2x2 block. The memory is laid out as follows:
  //  - Yplane, full size (each element represents a 1x1 block)
  //  - Uplane, quarter size (each element represents a 2x2 block)
  //  - Vplane, quarter size (each element represents a 2x2 block)
  DCHECK((coded_size.width() % 2 == 0) && (coded_size.height() % 2 == 0));
  return Create(format, coded_size, visible_rect, dimensions, std::move(handle),
                allocation_size, 0 /* y_offset */, coded_size.GetArea(),
                coded_size.GetArea() * 5 / 4, coded_size.width(),
                coded_size.width() / 2, coded_size.width() / 2, timestamp);
}

scoped_refptr<MojoSharedBufferVideoFrame>
MojoSharedBufferVideoFrame::CreateFromYUVFrame(const VideoFrame& frame) {
  DCHECK_EQ(VideoFrame::NumPlanes(frame.format()), 3u);

  const size_t y_stride = frame.stride(VideoFrame::kYPlane);
  const size_t u_stride = frame.stride(VideoFrame::kUPlane);
  const size_t v_stride = frame.stride(VideoFrame::kVPlane);

  const size_t y_size =
      VideoFrame::Rows(kYPlane, frame.format(), frame.coded_size().height()) *
      y_stride;
  const size_t u_size =
      VideoFrame::Rows(kUPlane, frame.format(), frame.coded_size().height()) *
      u_stride;
  const size_t v_size =
      VideoFrame::Rows(kVPlane, frame.format(), frame.coded_size().height()) *
      v_stride;

  size_t allocation_size = y_size + u_size + v_size;

  mojo::ScopedSharedBufferHandle handle =
      mojo::SharedBufferHandle::Create(allocation_size);

  // Computes the offset of planes in shared memory buffer.
  const size_t y_offset = 0u;
  const size_t u_offset = y_offset + y_size;
  const size_t v_offset = y_offset + y_size + u_size;

  // The data from |frame| may not be consecutive between planes. Copy data into
  // a shared memory buffer which is tightly packed. Padding inside each planes
  // are preserved.
  scoped_refptr<MojoSharedBufferVideoFrame> mojo_frame =
      MojoSharedBufferVideoFrame::Create(
          frame.format(), frame.coded_size(), frame.visible_rect(),
          frame.natural_size(), std::move(handle), allocation_size, y_offset,
          u_offset, v_offset, y_stride, u_stride, v_stride, frame.timestamp());

  // Copy Y plane.
  memcpy(mojo_frame->shared_buffer_data(),
         static_cast<const void*>(frame.data(VideoFrame::kYPlane)), y_size);

  // Copy U plane.
  memcpy(mojo_frame->shared_buffer_data() + u_offset,
         static_cast<const void*>(frame.data(VideoFrame::kUPlane)), u_size);

  // Copy V plane.
  memcpy(mojo_frame->shared_buffer_data() + v_offset,
         static_cast<const void*>(frame.data(VideoFrame::kVPlane)), v_size);

  // TODO(xingliu): Maybe also copy the alpha plane in
  // |MojoSharedBufferVideoFrame|. The alpha plane is ignored here, but
  // the |shared_memory| should contain the space for alpha plane.

  return mojo_frame;
}

// static
scoped_refptr<MojoSharedBufferVideoFrame> MojoSharedBufferVideoFrame::Create(
    VideoPixelFormat format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    mojo::ScopedSharedBufferHandle handle,
    size_t data_size,
    size_t y_offset,
    size_t u_offset,
    size_t v_offset,
    int32_t y_stride,
    int32_t u_stride,
    int32_t v_stride,
    base::TimeDelta timestamp) {
  if (!IsValidConfig(format, STORAGE_MOJO_SHARED_BUFFER, coded_size,
                     visible_rect, natural_size)) {
    LOG(DFATAL) << __func__ << " Invalid config. "
                << ConfigToString(format, STORAGE_MOJO_SHARED_BUFFER,
                                  coded_size, visible_rect, natural_size);
    return nullptr;
  }

  // Validate that the offsets and strides fit in the buffer.
  //
  // We can rely on coded_size.GetArea() being relatively small (compared to the
  // range of an int) due to the IsValidConfig() check above.
  //
  // TODO(sandersd): Allow non-sequential formats.
  if (NumPlanes(format) != 3) {
    DLOG(ERROR) << __func__ << " " << VideoPixelFormatToString(format)
                << " is not supported; only YUV formats are allowed";
    return nullptr;
  }

  if (y_stride < 0 || u_stride < 0 || v_stride < 0) {
    DLOG(ERROR) << __func__ << " Invalid stride";
    return nullptr;
  }

  // Compute the number of bytes needed on each row.
  const size_t y_row_bytes = RowBytes(kYPlane, format, coded_size.width());
  const size_t u_row_bytes = RowBytes(kUPlane, format, coded_size.width());
  const size_t v_row_bytes = RowBytes(kVPlane, format, coded_size.width());

  // Safe given sizeof(size_t) >= sizeof(int32_t).
  size_t y_stride_size_t = y_stride;
  size_t u_stride_size_t = u_stride;
  size_t v_stride_size_t = v_stride;
  if (y_stride_size_t < y_row_bytes || u_stride_size_t < u_row_bytes ||
      v_stride_size_t < v_row_bytes) {
    DLOG(ERROR) << __func__ << " Invalid stride";
    return nullptr;
  }

  const size_t y_rows = Rows(kYPlane, format, coded_size.height());
  const size_t u_rows = Rows(kUPlane, format, coded_size.height());
  const size_t v_rows = Rows(kVPlane, format, coded_size.height());

  // The last row only needs RowBytes() and not a full stride. This is to avoid
  // problems if the U and V data is interleaved (where |stride| is double the
  // number of bytes actually needed).
  base::CheckedNumeric<size_t> y_bound = base::CheckAdd(
      y_offset, base::CheckMul(base::CheckSub(y_rows, 1), y_stride_size_t),
      y_row_bytes);
  base::CheckedNumeric<size_t> u_bound = base::CheckAdd(
      u_offset, base::CheckMul(base::CheckSub(u_rows, 1), u_stride_size_t),
      u_row_bytes);
  base::CheckedNumeric<size_t> v_bound = base::CheckAdd(
      v_offset, base::CheckMul(base::CheckSub(v_rows, 1), v_stride_size_t),
      v_row_bytes);

  if (!y_bound.IsValid() || !u_bound.IsValid() || !v_bound.IsValid() ||
      y_bound.ValueOrDie() > data_size || u_bound.ValueOrDie() > data_size ||
      v_bound.ValueOrDie() > data_size) {
    DLOG(ERROR) << __func__ << " Invalid offset";
    return nullptr;
  }
  auto layout = VideoFrameLayout::CreateWithStrides(
      format, coded_size, {y_stride, u_stride, v_stride});
  if (!layout) {
    return nullptr;
  }
  // Now allocate the frame and initialize it.
  scoped_refptr<MojoSharedBufferVideoFrame> frame(
      new MojoSharedBufferVideoFrame(*layout, visible_rect, natural_size,
                                     std::move(handle), data_size, timestamp));
  if (!frame->Init(y_offset, u_offset, v_offset)) {
    DLOG(ERROR) << __func__ << " MojoSharedBufferVideoFrame::Init failed.";
    return nullptr;
  }

  return frame;
}

MojoSharedBufferVideoFrame::MojoSharedBufferVideoFrame(
    const VideoFrameLayout& layout,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    mojo::ScopedSharedBufferHandle handle,
    size_t mapped_size,
    base::TimeDelta timestamp)
    : VideoFrame(layout,
                 STORAGE_MOJO_SHARED_BUFFER,
                 visible_rect,
                 natural_size,
                 timestamp),
      shared_buffer_handle_(std::move(handle)),
      shared_buffer_size_(mapped_size) {
  DCHECK(shared_buffer_handle_.is_valid());
}

bool MojoSharedBufferVideoFrame::Init(size_t y_offset,
                                      size_t u_offset,
                                      size_t v_offset) {
  DCHECK(!shared_buffer_mapping_);
  shared_buffer_mapping_ = shared_buffer_handle_->Map(shared_buffer_size_);
  if (!shared_buffer_mapping_)
    return false;

  offsets_[kYPlane] = y_offset;
  offsets_[kUPlane] = u_offset;
  offsets_[kVPlane] = v_offset;
  set_data(kYPlane, shared_buffer_data() + y_offset);
  set_data(kUPlane, shared_buffer_data() + u_offset);
  set_data(kVPlane, shared_buffer_data() + v_offset);
  return true;
}

MojoSharedBufferVideoFrame::~MojoSharedBufferVideoFrame() {
  // Call |mojo_shared_buffer_done_cb_| to take ownership of
  // |shared_buffer_handle_|.
  if (mojo_shared_buffer_done_cb_)
    mojo_shared_buffer_done_cb_.Run(std::move(shared_buffer_handle_),
                                    shared_buffer_size_);
}

size_t MojoSharedBufferVideoFrame::PlaneOffset(size_t plane) const {
  DCHECK(IsValidPlane(format(), plane));
  return offsets_[plane];
}

void MojoSharedBufferVideoFrame::SetMojoSharedBufferDoneCB(
    const MojoSharedBufferDoneCB& mojo_shared_buffer_done_cb) {
  mojo_shared_buffer_done_cb_ = mojo_shared_buffer_done_cb;
}

const mojo::SharedBufferHandle& MojoSharedBufferVideoFrame::Handle() const {
  return shared_buffer_handle_.get();
}

size_t MojoSharedBufferVideoFrame::MappedSize() const {
  return shared_buffer_size_;
}

}  // namespace media
