// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/generic_dmabuf_video_frame_mapper.h"

#include <sys/mman.h>

#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "media/gpu/chromeos/chromeos_compressed_gpu_memory_buffer_video_frame_utils.h"
#include "media/gpu/macros.h"
#include "third_party/libyuv/include/libyuv.h"

namespace media {

namespace {

// The format coming in is P010, but it's tagged as P016LE.  The
// difference being that P010 uses the MSB while P016LE uses the LSB.
// The conversion is done to YUV420P10 aka I010 which is a tri-planar
// format that will be used for md5 sum computations and writing out to
// disk.
scoped_refptr<VideoFrame> ConvertYUV420P10Frame(
    scoped_refptr<const VideoFrame> src_video_frame,
    uint8_t* plane_addrs[VideoFrame::kMaxPlanes]) {
  const size_t kNumPlanesYUV420P10 =
      VideoFrame::NumPlanes(PIXEL_FORMAT_YUV420P10);
  std::vector<std::unique_ptr<uint16_t[]>> yuv420p10_buffers(
      kNumPlanesYUV420P10);

  const auto& visible_rect = src_video_frame->visible_rect();
  const int32_t src_stride = src_video_frame->stride(VideoFrame::kYPlane);
  std::vector<int32_t> dst_strides = {src_stride, src_stride >> 1,
                                      src_stride >> 1};
  const auto layout = VideoFrameLayout::CreateWithStrides(
      PIXEL_FORMAT_YUV420P10, src_video_frame->natural_size(), dst_strides);

  const uint16_t* src_plane_y =
      reinterpret_cast<const uint16_t*>(plane_addrs[0]);
  const uint16_t* src_plane_uv =
      reinterpret_cast<const uint16_t*>(plane_addrs[1]);

  for (size_t i = 0; i < kNumPlanesYUV420P10; i++) {
    const size_t plane_height =
        VideoFrame::Rows(i, PIXEL_FORMAT_YUV420P10, visible_rect.height());
    const size_t plane_size_16bit_words =
        (layout->planes()[i].stride * plane_height) >> 1;

    yuv420p10_buffers[i] = std::make_unique<uint16_t[]>(plane_size_16bit_words);
    plane_addrs[i] = reinterpret_cast<uint8_t*>(yuv420p10_buffers[i].get());
  }

  // VideoFrame stores strides in bytes per line. libyuv expects strides
  // in samples per line. Dividing bytes per line by two gives samples per line
  libyuv::P010ToI010(
      src_plane_y, src_video_frame->stride(VideoFrame::kYPlane) >> 1,
      src_plane_uv, src_video_frame->stride(VideoFrame::kUVPlane) >> 1,
      yuv420p10_buffers[0].get(),
      layout->planes()[VideoFrame::kYPlane].stride >> 1,
      yuv420p10_buffers[1].get(),
      layout->planes()[VideoFrame::kUPlane].stride >> 1,
      yuv420p10_buffers[2].get(),
      layout->planes()[VideoFrame::kVPlane].stride >> 1, visible_rect.width(),
      visible_rect.height());

  scoped_refptr<VideoFrame> video_frame =
      VideoFrame::WrapExternalYuvDataWithLayout(
          layout.value(), visible_rect, visible_rect.size(), plane_addrs[0],
          plane_addrs[1], plane_addrs[2], src_video_frame->timestamp());

  for (auto&& buffer : yuv420p10_buffers) {
    video_frame->AddDestructionObserver(
        base::DoNothingWithBoundArgs(std::move(buffer)));
  }

  return video_frame;
}

uint8_t* Mmap(const size_t length, const int fd, int permissions) {
  void* addr = mmap(nullptr, length, permissions, MAP_SHARED, fd, 0u);

  if (addr == MAP_FAILED) {
    return nullptr;
  }

  return static_cast<uint8_t*>(addr);
}

void MunmapBuffers(const std::vector<std::pair<uint8_t*, size_t>>& chunks,
                   scoped_refptr<const VideoFrame> video_frame) {
  for (const auto& chunk : chunks) {
    DLOG_IF(ERROR, !chunk.first) << "Pointer to be released is nullptr.";
    munmap(chunk.first, chunk.second);
  }
}

// Create VideoFrame whose dtor unmaps memory in mapped planes referred
// by |plane_addrs|. |plane_addrs| are addresses to (Y, U, V) in this order.
// |chunks| is the vector of pair of (address, size) to be called in munmap().
// |src_video_frame| is the video frame that owns dmabufs to the mapped planes.
scoped_refptr<VideoFrame> CreateMappedVideoFrame(
    scoped_refptr<const VideoFrame> src_video_frame,
    uint8_t* plane_addrs[VideoFrame::kMaxPlanes],
    const std::vector<std::pair<uint8_t*, size_t>>& chunks) {
  scoped_refptr<VideoFrame> video_frame;

  const auto& layout = src_video_frame->layout();
  const auto& visible_rect = src_video_frame->visible_rect();

  if (IsYuvPlanar(layout.format())) {
    if (src_video_frame->format() == PIXEL_FORMAT_P016LE) {
      video_frame = ConvertYUV420P10Frame(src_video_frame, plane_addrs);
    } else {
      video_frame = VideoFrame::WrapExternalYuvDataWithLayout(
          layout, visible_rect, visible_rect.size(), plane_addrs[0],
          plane_addrs[1], plane_addrs[2], src_video_frame->timestamp());
    }
  } else if (VideoFrame::NumPlanes(layout.format()) == 1) {
    video_frame = VideoFrame::WrapExternalDataWithLayout(
        layout, visible_rect, visible_rect.size(), plane_addrs[0],
        layout.planes()[0].size, src_video_frame->timestamp());
  }
  if (!video_frame) {
    MunmapBuffers(chunks, /*video_frame=*/nullptr);
    return nullptr;
  }

  // Pass org_video_frame so that it outlives video_frame.
  video_frame->AddDestructionObserver(
      base::BindOnce(MunmapBuffers, chunks, std::move(src_video_frame)));

  return video_frame;
}

bool IsFormatSupported(VideoPixelFormat format) {
  constexpr VideoPixelFormat supported_formats[] = {
      // RGB pixel formats.
      PIXEL_FORMAT_ABGR,
      PIXEL_FORMAT_ARGB,
      PIXEL_FORMAT_XBGR,

      // YUV pixel formats.
      PIXEL_FORMAT_I420,
      PIXEL_FORMAT_NV12,
      PIXEL_FORMAT_YV12,
      PIXEL_FORMAT_P016LE,

      // Compressed format.
      PIXEL_FORMAT_MJPEG,
  };
  return base::Contains(supported_formats, format);
}

}  // namespace

// static
std::unique_ptr<GenericDmaBufVideoFrameMapper>
GenericDmaBufVideoFrameMapper::Create(VideoPixelFormat format) {
  if (!IsFormatSupported(format)) {
    VLOGF(1) << "Unsupported format: " << format;
    return nullptr;
  }
  return base::WrapUnique(new GenericDmaBufVideoFrameMapper(format));
}

GenericDmaBufVideoFrameMapper::GenericDmaBufVideoFrameMapper(
    VideoPixelFormat format)
    : VideoFrameMapper(format) {}

scoped_refptr<VideoFrame> GenericDmaBufVideoFrameMapper::Map(
    scoped_refptr<const VideoFrame> video_frame,
    int permissions) const {
  if (!video_frame) {
    LOG(ERROR) << "Video frame is nullptr";
    return nullptr;
  }

  if (video_frame->storage_type() != VideoFrame::StorageType::STORAGE_DMABUFS) {
    VLOGF(1) << "VideoFrame's storage type is not DMABUF: "
             << video_frame->storage_type();
    return nullptr;
  }

  if (IsIntelMediaCompressedModifier(video_frame->layout().modifier())) {
    VLOGF(1)
        << "This mapper doesn't support Intel media compressed VideoFrames";
    return nullptr;
  }

  if (video_frame->format() != format_) {
    VLOGF(1) << "Unexpected format: " << video_frame->format()
             << ", expected: " << format_;
    return nullptr;
  }

  // Map all buffers from their start address.
  const auto& planes = video_frame->layout().planes();
  if (planes[0].offset != 0) {
    VLOGF(1) << "The offset of the first plane is not zero";
    return nullptr;
  }

  // Always prepare VideoFrame::kMaxPlanes addresses for planes initialized by
  // nullptr. This enables to specify nullptr to redundant plane, for pixel
  // format whose number of planes are less than VideoFrame::kMaxPlanes.
  uint8_t* plane_addrs[VideoFrame::kMaxPlanes] = {};
  const size_t num_planes = planes.size();
  const auto& dmabuf_fds = video_frame->DmabufFds();
  std::vector<std::pair<uint8_t*, size_t>> chunks;
  DCHECK_EQ(dmabuf_fds.size(), num_planes);
  for (size_t i = 0; i < num_planes;) {
    size_t next_buf = i + 1;
    // Search the index of the plane from which the next buffer starts.
    while (next_buf < num_planes && planes[next_buf].offset != 0)
      next_buf++;

    // Map the current buffer.
    const auto& last_plane = planes[next_buf - 1];

    size_t mapped_size = 0;
    if (!base::CheckAdd<size_t>(last_plane.offset, last_plane.size)
             .AssignIfValid(&mapped_size)) {
      VLOGF(1) << "Overflow happens with offset=" << last_plane.offset
               << " + size=" << last_plane.size;
      MunmapBuffers(chunks, /*video_frame=*/nullptr);
      return nullptr;
    }

    uint8_t* mapped_addr = Mmap(mapped_size, dmabuf_fds[i].get(), permissions);
    if (!mapped_addr) {
      VLOGF(1) << "nullptr returned by Mmap";
      MunmapBuffers(chunks, /*video_frame=*/nullptr);
      return nullptr;
    }

    chunks.emplace_back(mapped_addr, mapped_size);
    for (size_t j = i; j < next_buf; ++j)
      plane_addrs[j] = mapped_addr + planes[j].offset;

    i = next_buf;
  }

  return CreateMappedVideoFrame(std::move(video_frame), plane_addrs, chunks);
}

}  // namespace media
