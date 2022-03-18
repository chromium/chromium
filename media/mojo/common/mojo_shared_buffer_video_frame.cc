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
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"

namespace media {

// static
scoped_refptr<MojoSharedBufferVideoFrame>
MojoSharedBufferVideoFrame::CreateDefaultForTesting(
    const VideoPixelFormat format,
    const gfx::Size& dimensions,
    base::TimeDelta timestamp) {
  DCHECK(format == PIXEL_FORMAT_I420 || format == PIXEL_FORMAT_NV12);
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
  auto region = base::UnsafeSharedMemoryRegion::Create(allocation_size);
  if (!region.IsValid()) {
    DLOG(ERROR) << __func__ << " Unable to allocate memory.";
    return nullptr;
  }

  // As both formats are 4:2:0, the U and V (or UV plane) have samples for each
  // 2x2 block.
  DCHECK((coded_size.width() % 2 == 0) && (coded_size.height() % 2 == 0));

  if (format == PIXEL_FORMAT_I420) {
    // Create and initialize the frame. As this is I420 format, the U and V
    // planes have samples for each 2x2 block. The memory is laid out as
    // follows:
    //  - Yplane, full size (each element represents a 1x1 block)
    //  - Uplane, quarter size (each element represents a 2x2 block)
    //  - Vplane, quarter size (each element represents a 2x2 block)
    const uint32_t offsets[] = {
        0 /* y_offset */, static_cast<uint32_t>(coded_size.GetArea()),
        static_cast<uint32_t>(coded_size.GetArea() * 5 / 4)};
    const int32_t strides[] = {coded_size.width(), coded_size.width() / 2,
                               coded_size.width() / 2};
    return Create(format, coded_size, visible_rect, dimensions,
                  std::move(region), offsets, strides, timestamp);
  } else {
    // |format| is PIXEL_FORMAT_NV12.
    // Create and initialize the frame. As this is NV12 format, the UV plane
    // has interleaved U & V samples for each 2x2 block. The memory is laid out
    // as follows:
    //  - Yplane, full size (each element represents a 1x1 block)
    //  - UVplane, full width, half height (each pair represents a 2x2 block)
    const uint32_t offsets[] = {0 /* y_offset */,
                                static_cast<uint32_t>(coded_size.GetArea())};
    const int32_t strides[] = {coded_size.width(), coded_size.width()};
    return Create(format, coded_size, visible_rect, dimensions,
                  std::move(region), offsets, strides, timestamp);
  }
}

scoped_refptr<MojoSharedBufferVideoFrame>
MojoSharedBufferVideoFrame::CreateFromYUVFrame(VideoFrame& frame) {
  size_t num_planes = VideoFrame::NumPlanes(frame.format());
  DCHECK_LE(num_planes, 3u);
  DCHECK_GE(num_planes, 2u);

  std::vector<uint32_t> offsets(num_planes);
  std::vector<int32_t> strides(num_planes);
  std::vector<size_t> sizes(num_planes);
  size_t aggregate_size = 0;
  for (size_t i = 0; i < num_planes; ++i) {
    strides[i] = frame.stride(i);
    offsets[i] = aggregate_size;
    sizes[i] =
        VideoFrame::Rows(i, frame.format(), frame.coded_size().height()) *
        strides[i];
    aggregate_size += sizes[i];
  }

  auto region = base::UnsafeSharedMemoryRegion::Create(aggregate_size);
  if (!region.IsValid()) {
    DLOG(ERROR) << "Can't create new frame backing memory";
    return nullptr;
  }
  base::WritableSharedMemoryMapping dst_mapping = region.Map();
  if (!dst_mapping.IsValid()) {
    DLOG(ERROR) << "Can't create map frame backing memory";
    return nullptr;
  }

  // The data from |frame| may not be consecutive between planes. Copy data into
  // a shared memory buffer which is tightly packed. Padding inside each planes
  // are preserved.
  scoped_refptr<MojoSharedBufferVideoFrame> mojo_frame =
      MojoSharedBufferVideoFrame::Create(
          frame.format(), frame.coded_size(), frame.visible_rect(),
          frame.natural_size(), std::move(region), offsets, strides,
          frame.timestamp());
  CHECK(!!mojo_frame);

  // If the source memory region is a shared memory region we must map it too.
  base::WritableSharedMemoryMapping src_mapping;
  if (frame.storage_type() == VideoFrame::STORAGE_SHMEM) {
    if (!frame.shm_region()->IsValid()) {
      DLOG(ERROR) << "Invalid source shared memory region";
      return nullptr;
    }
    src_mapping = frame.shm_region()->Map();
    if (!src_mapping.IsValid()) {
      DLOG(ERROR) << "Can't map source shared memory region";
      return nullptr;
    }
  }

  // Copy plane data while mappings are in scope.
  for (size_t i = 0; i < num_planes; ++i) {
    memcpy(mojo_frame->shared_buffer_data() + offsets[i],
           static_cast<const void*>(frame.data(i)), sizes[i]);
  }

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
    base::UnsafeSharedMemoryRegion region,
    base::span<const uint32_t> offsets,
    base::span<const int32_t> strides,
    base::TimeDelta timestamp) {
  if (!IsValidConfig(format, STORAGE_MOJO_SHARED_BUFFER, coded_size,
                     visible_rect, natural_size)) {
    LOG(DFATAL) << __func__ << " Invalid config. "
                << ConfigToString(format, STORAGE_MOJO_SHARED_BUFFER,
                                  coded_size, visible_rect, natural_size);
    return nullptr;
  }

  // Validate that the format has the proper plane count and that it matches the
  // offsets/strides array sizes passed in.
  size_t num_planes = NumPlanes(format);
  if (num_planes != 3 && num_planes != 2) {
    DLOG(ERROR) << __func__ << " " << VideoPixelFormatToString(format)
                << " is not supported; only bi/tri-planar formats are allowed";
    return nullptr;
  }
  if (num_planes != offsets.size() || num_planes != strides.size()) {
    DLOG(ERROR) << __func__ << " offsets and strides length must match number "
                << "of planes";
    return nullptr;
  }

  // Validate that the offsets and strides fit in the buffer.
  //
  // We can rely on coded_size.GetArea() being relatively small (compared to the
  // range of an int) due to the IsValidConfig() check above.
  //
  // TODO(sandersd): Allow non-sequential formats.
  const size_t data_size = region.GetSize();
  std::vector<ColorPlaneLayout> planes(num_planes);
  for (size_t i = 0; i < num_planes; ++i) {
    if (strides[i] < 0) {
      DLOG(ERROR) << __func__ << " Invalid stride";
      return nullptr;
    }

    // Compute the number of bytes needed on each row.
    const size_t row_bytes = RowBytes(i, format, coded_size.width());

    // Safe given sizeof(size_t) >= sizeof(int32_t).
    size_t stride_size_t = strides[i];
    if (stride_size_t < row_bytes) {
      DLOG(ERROR) << __func__ << " Invalid stride";
      return nullptr;
    }
    const size_t rows = Rows(i, format, coded_size.height());

    // The last row only needs RowBytes() and not a full stride. This is to
    // avoid problems if the U and V data is interleaved (where |stride| is
    // double the number of bytes actually needed).
    base::CheckedNumeric<size_t> bound = base::CheckAdd(
        offsets[i], base::CheckMul(base::CheckSub(rows, 1), stride_size_t),
        row_bytes);
    if (!bound.IsValid() || bound.ValueOrDie() > data_size) {
      DLOG(ERROR) << __func__ << " Invalid offset";
      return nullptr;
    }

    planes[i].stride = strides[i];
    planes[i].offset = offsets[i];
    planes[i].size = i + 1 < num_planes ? offsets[i + 1] - offsets[i]
                                        : data_size - offsets.back();
  }

  auto layout =
      VideoFrameLayout::CreateWithPlanes(format, coded_size, std::move(planes));
  if (!layout) {
    return nullptr;
  }
  // Now allocate the frame and initialize it.
  scoped_refptr<MojoSharedBufferVideoFrame> frame(
      new MojoSharedBufferVideoFrame(*layout, visible_rect, natural_size,
                                     std::move(region), timestamp));
  if (!frame->Init(offsets)) {
    DLOG(ERROR) << __func__ << " MojoSharedBufferVideoFrame::Init failed.";
    return nullptr;
  }

  return frame;
}

MojoSharedBufferVideoFrame::MojoSharedBufferVideoFrame(
    const VideoFrameLayout& layout,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::UnsafeSharedMemoryRegion region,
    base::TimeDelta timestamp)
    : VideoFrame(layout,
                 STORAGE_MOJO_SHARED_BUFFER,
                 visible_rect,
                 natural_size,
                 timestamp),
      region_(std::move(region)) {
  DCHECK(region_.IsValid());
}

bool MojoSharedBufferVideoFrame::Init(base::span<const uint32_t> offsets) {
  DCHECK(!mapping_.IsValid());
  mapping_ = region_.Map();
  if (!mapping_.IsValid())
    return false;
  const size_t num_planes = NumPlanes(format());
  DCHECK_EQ(offsets.size(), num_planes);
  for (size_t i = 0; i < num_planes; ++i) {
    offsets_[i] = offsets[i];
    set_data(i, shared_buffer_data() + offsets[i]);
  }
  return true;
}

MojoSharedBufferVideoFrame::~MojoSharedBufferVideoFrame() {
  // Call |mojo_shared_buffer_done_cb_| to take ownership of
  // |shared_buffer_handle_|.
  if (mojo_shared_buffer_done_cb_) {
    std::move(mojo_shared_buffer_done_cb_).Run(std::move(region_));
  }
}

size_t MojoSharedBufferVideoFrame::PlaneOffset(size_t plane) const {
  DCHECK(IsValidPlane(format(), plane));
  return offsets_[plane];
}

void MojoSharedBufferVideoFrame::SetMojoSharedBufferDoneCB(
    MojoSharedBufferDoneCB mojo_shared_buffer_done_cb) {
  mojo_shared_buffer_done_cb_ = std::move(mojo_shared_buffer_done_cb);
}

}  // namespace media
