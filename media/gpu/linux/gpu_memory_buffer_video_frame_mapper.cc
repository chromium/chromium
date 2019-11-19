// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/linux/gpu_memory_buffer_video_frame_mapper.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "media/gpu/macros.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace media {
// static
std::unique_ptr<GpuMemoryBufferVideoFrameMapper>
GpuMemoryBufferVideoFrameMapper::Create(VideoPixelFormat format) {
  return base::WrapUnique(new GpuMemoryBufferVideoFrameMapper(format));
}

GpuMemoryBufferVideoFrameMapper::GpuMemoryBufferVideoFrameMapper(
    VideoPixelFormat format)
    : VideoFrameMapper(format) {}

scoped_refptr<VideoFrame> GpuMemoryBufferVideoFrameMapper::Map(
    scoped_refptr<const VideoFrame> video_frame) const {
  if (!video_frame) {
    LOG(ERROR) << "Video frame is nullptr";
    return nullptr;
  }

  if (video_frame->storage_type() !=
      VideoFrame::StorageType::STORAGE_GPU_MEMORY_BUFFER) {
    VLOGF(1) << "VideoFrame's storage type is not GPU_MEMORY_BUFFER: "
             << video_frame->storage_type();
    return nullptr;
  }

  if (video_frame->format() != format_) {
    VLOGF(1) << "Unexpected format: " << video_frame->format()
             << ", expected: " << format_;
    return nullptr;
  }

  gfx::GpuMemoryBuffer* gmb = video_frame->GetGpuMemoryBuffer();
  if (!gmb)
    return nullptr;

  if (!gmb->Map()) {
    VLOGF(1) << "Failed to map GpuMemoryBuffer";
    return nullptr;
  }

  const size_t num_planes = VideoFrame::NumPlanes(format_);
  uint8_t* plane_addrs[VideoFrame::kMaxPlanes] = {};
  for (size_t i = 0; i < num_planes; i++)
    plane_addrs[i] = static_cast<uint8_t*>(gmb->memory(i));

  scoped_refptr<VideoFrame> mapped_frame;
  if (IsYuvPlanar(format_)) {
    mapped_frame = VideoFrame::WrapExternalYuvDataWithLayout(
        video_frame->layout(), video_frame->visible_rect(),
        video_frame->natural_size(), plane_addrs[0], plane_addrs[1],
        plane_addrs[2], video_frame->timestamp());
  } else if (num_planes == 1) {
    size_t buffer_size = VideoFrame::AllocationSize(
        format_, gfx::Size(gmb->stride(0), gmb->GetSize().height()));
    mapped_frame = VideoFrame::WrapExternalDataWithLayout(
        video_frame->layout(), video_frame->visible_rect(),
        video_frame->natural_size(), plane_addrs[0], buffer_size,
        video_frame->timestamp());
  }

  if (!mapped_frame)
    return nullptr;

  // Pass |video_frame| so that it outlives |mapped_frame| and the mapped buffer
  // is unmapped on destruction.
  mapped_frame->AddDestructionObserver(base::BindOnce(
      [](scoped_refptr<const VideoFrame> frame) {
        DCHECK(frame->HasGpuMemoryBuffer());
        frame->GetGpuMemoryBuffer()->Unmap();
      },
      std::move(video_frame)));
  return mapped_frame;
}
}  // namespace media
