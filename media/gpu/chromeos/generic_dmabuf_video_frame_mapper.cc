// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/chromeos/generic_dmabuf_video_frame_mapper.h"

#include <sys/mman.h>

#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "media/gpu/macros.h"

namespace media {

namespace {

uint8_t* Mmap(const size_t length, const int fd, int permissions) {
  void* addr = mmap(nullptr, length, permissions, MAP_SHARED, fd, 0u);

  if (addr == MAP_FAILED) {
    return nullptr;
  }

  return static_cast<uint8_t*>(addr);
}

void MunmapBuffers(const std::vector<std::pair<uint8_t*, size_t>>& chunks,
                   scoped_refptr<const FrameResource> video_frame) {
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
    scoped_refptr<const FrameResource> src_video_frame,
    uint8_t* plane_addrs[VideoFrame::kMaxPlanes],
    const std::vector<std::pair<uint8_t*, size_t>>& chunks) {
  scoped_refptr<VideoFrame> video_frame;

  const auto& layout = src_video_frame->layout();
  const auto& visible_rect = src_video_frame->visible_rect();
  if (IsYuvPlanar(layout.format())) {
    video_frame = VideoFrame::WrapExternalYuvDataWithLayout(
        layout, visible_rect, visible_rect.size(), plane_addrs[0],
        plane_addrs[1], plane_addrs[2], src_video_frame->timestamp());
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
      PIXEL_FORMAT_P010LE,

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

scoped_refptr<VideoFrame> GenericDmaBufVideoFrameMapper::MapFrame(
    scoped_refptr<const FrameResource> video_frame,
    int permissions) {
  if (!video_frame) {
    LOG(ERROR) << "Video frame is nullptr";
    return nullptr;
  }

  if (video_frame->storage_type() != VideoFrame::StorageType::STORAGE_DMABUFS) {
    VLOGF(1) << "VideoFrame's storage type is not DMABUF: "
             << video_frame->storage_type();
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
  std::vector<std::pair<uint8_t*, size_t>> chunks;
  DCHECK_EQ(video_frame->NumDmabufFds(), num_planes);
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

    uint8_t* mapped_addr =
        Mmap(mapped_size, video_frame->GetDmabufFd(i), permissions);
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
