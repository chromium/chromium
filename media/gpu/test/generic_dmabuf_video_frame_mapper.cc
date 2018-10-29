// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/generic_dmabuf_video_frame_mapper.h"

#include <sys/mman.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "base/bind.h"

#define VLOGF(level) VLOG(level) << __func__ << "(): "

namespace media {
namespace test {

namespace {

constexpr size_t kNumOfYUVPlanes = 3;

uint8_t* Mmap(const size_t length, const int fd) {
  void* addr = mmap(nullptr, length, PROT_READ, MAP_PRIVATE, fd, 0u);
  if (addr == MAP_FAILED) {
    VLOGF(1) << "Failed to mmap.";
    return nullptr;
  }
  return static_cast<uint8_t*>(addr);
}

void MunmapBuffers(const std::vector<std::pair<uint8_t*, size_t>>& chunks,
                   scoped_refptr<VideoFrame> video_frame) {
  for (const auto& chunk : chunks) {
    DLOG_IF(ERROR, !chunk.first) << "Pointer to be released is nullptr.";
    munmap(chunk.first, chunk.second);
  }
}

// Create VideoFrame whose dtor deallocates memory in mapped planes referred
// by |plane_addrs|. |plane_addrs| are addresses to (Y, U, V) in this order.
// |chunks| is the vector of pair of (address, size) to be called in munmap().
// |src_video_frame| is the video frame that owns dmabufs to the mapped planes.
scoped_refptr<VideoFrame> CreateMappedVideoFrame(
    const VideoFrameLayout& layout,
    const gfx::Rect& visible_rect,
    uint8_t* plane_addrs[kNumOfYUVPlanes],
    const std::vector<std::pair<uint8_t*, size_t>>& chunks,
    scoped_refptr<VideoFrame> src_video_frame) {
  int32_t strides[kNumOfYUVPlanes] = {};
  for (size_t i = 0; i < layout.num_planes(); i++) {
    strides[i] = layout.planes()[i].stride;
  }
  if (layout.format() == PIXEL_FORMAT_YV12) {
    // Swap the address of U and V planes, because the order U and V planes is
    // reversed in YV12.
    std::swap(plane_addrs[1], plane_addrs[2]);
  }
  auto video_frame = VideoFrame::WrapExternalYuvData(
      layout.format(), layout.coded_size(), visible_rect, visible_rect.size(),
      strides[0], strides[1], strides[2], plane_addrs[0], plane_addrs[1],
      plane_addrs[2], base::TimeDelta());
  if (!video_frame) {
    return nullptr;
  }

  // Pass org_video_frame so that it outlives video_frame.
  video_frame->AddDestructionObserver(
      base::BindOnce(MunmapBuffers, chunks, std::move(src_video_frame)));
  return video_frame;
}

}  // namespace

scoped_refptr<VideoFrame> GenericDmaBufVideoFrameMapper::Map(
    scoped_refptr<VideoFrame> video_frame) const {
  if (video_frame->storage_type() != VideoFrame::StorageType::STORAGE_DMABUFS) {
    VLOGF(1) << "VideoFrame's storage type is not DMABUF: "
             << video_frame->storage_type();
    return nullptr;
  }

  const VideoFrameLayout& layout = video_frame->layout();
  const VideoPixelFormat pixel_format = layout.format();
  // GenericDmaBufVideoFrameMapper only supports NV12 and YV12 for output
  // picture format.
  if (pixel_format != PIXEL_FORMAT_NV12 && pixel_format != PIXEL_FORMAT_YV12) {
    NOTIMPLEMENTED() << " Unsupported PixelFormat: " << pixel_format;
    return nullptr;
  }

  // Map all buffers from their start address.
  const auto& dmabuf_fds = video_frame->DmabufFds();
  std::vector<std::pair<uint8_t*, size_t>> chunks;
  const auto& buffer_sizes = layout.buffer_sizes();
  std::vector<uint8_t*> buffer_addrs(buffer_sizes.size(), nullptr);
  DCHECK_EQ(buffer_addrs.size(), dmabuf_fds.size());
  DCHECK_LE(buffer_addrs.size(), kNumOfYUVPlanes);
  for (size_t i = 0; i < dmabuf_fds.size(); i++) {
    buffer_addrs[i] = Mmap(buffer_sizes[i], dmabuf_fds[i].get());
    if (!buffer_addrs[i]) {
      MunmapBuffers(chunks, std::move(video_frame));
      return nullptr;
    }
    chunks.emplace_back(buffer_addrs[i], buffer_sizes[i]);
  }

  // Always prepare 3 addresses for planes initialized by nullptr.
  // This enables to specify nullptr to redundant plane, for pixel format whose
  // number of planes are less than 3.
  const auto& planes = layout.planes();
  const size_t num_of_planes = layout.num_planes();
  uint8_t* plane_addrs[kNumOfYUVPlanes] = {};
  if (dmabuf_fds.size() == 1) {
    for (size_t i = 0; i < num_of_planes; i++) {
      plane_addrs[i] = buffer_addrs[0] + planes[i].offset;
    }
  } else {
    for (size_t i = 0; i < num_of_planes; i++) {
      plane_addrs[i] = buffer_addrs[i] + planes[i].offset;
    }
  }
  return CreateMappedVideoFrame(layout, video_frame->visible_rect(),
                                plane_addrs, chunks, std::move(video_frame));
}

}  // namespace test
}  // namespace media
