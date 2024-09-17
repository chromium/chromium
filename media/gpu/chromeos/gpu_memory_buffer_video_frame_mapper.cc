// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/chromeos/gpu_memory_buffer_video_frame_mapper.h"

#include <sys/mman.h>

#include "base/functional/bind.h"
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

scoped_refptr<VideoFrame> GpuMemoryBufferVideoFrameMapper::MapFrame(
    scoped_refptr<const FrameResource> video_frame,
    int permissions) {
  if (!video_frame) {
    LOG(ERROR) << "Video frame is nullptr";
    return nullptr;
  }

  if (!(permissions & PROT_READ && permissions & PROT_WRITE)) {
    LOG(ERROR) << "GPU Memory Buffer must be mapped read/write.";
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

  auto scoped_mapping = video_frame->MapGMBOrSharedImage();
  if (!scoped_mapping) {
    VLOGF(1) << "Failed to get the mapped memory.";
    return nullptr;
  }

  const size_t num_planes = VideoFrame::NumPlanes(format_);
  uint8_t* plane_addrs[VideoFrame::kMaxPlanes] = {};
  for (size_t i = 0; i < num_planes; i++)
    plane_addrs[i] = scoped_mapping->Memory(i);

  scoped_refptr<VideoFrame> mapped_frame;
  if (IsYuvPlanar(format_)) {
    mapped_frame = VideoFrame::WrapExternalYuvDataWithLayout(
        video_frame->layout(), video_frame->visible_rect(),
        video_frame->natural_size(), plane_addrs[0], plane_addrs[1],
        plane_addrs[2], video_frame->timestamp());
  } else if (num_planes == 1) {
    size_t buffer_size = VideoFrame::AllocationSize(
        format_,
        gfx::Size(scoped_mapping->Stride(0), scoped_mapping->Size().height()));
    mapped_frame = VideoFrame::WrapExternalDataWithLayout(
        video_frame->layout(), video_frame->visible_rect(),
        video_frame->natural_size(), plane_addrs[0], buffer_size,
        video_frame->timestamp());
  }

  if (!mapped_frame) {
    return nullptr;
  }

  mapped_frame->set_color_space(video_frame->ColorSpace());
  mapped_frame->metadata().MergeMetadataFrom(video_frame->metadata());

  // Pass |video_frame| so that it outlives |mapped_frame| and the mapped buffer
  // is unmapped on destruction.
  mapped_frame->AddDestructionObserver(base::BindOnce(
      [](scoped_refptr<const FrameResource> frame,
         std::unique_ptr<VideoFrame::ScopedMapping> scoped_mapping) {
        CHECK(scoped_mapping);
        // The VideoFrame::ScopedMapping must be destroyed before the
        // FrameResource that produced it in order to avoid dangling pointers.
        scoped_mapping.reset();
      },
      std::move(video_frame), std::move(scoped_mapping)));
  return mapped_frame;
}
}  // namespace media
