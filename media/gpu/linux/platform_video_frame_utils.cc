// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/linux/platform_video_frame_utils.h"

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/scoped_file.h"
#include "build/build_config.h"
#include "media/base/color_plane_layout.h"
#include "media/base/format_utils.h"
#include "media/base/scopedfd_helper.h"
#include "media/base/video_frame_layout.h"
#include "media/gpu/buffer_validation.h"
#include "media/gpu/macros.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"
#include "ui/gfx/native_pixmap.h"

#if defined(OS_LINUX)
#include "gpu/ipc/common/gpu_client_ids.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#endif  // defined(OS_LINUX)

namespace media {

namespace {

#if defined(OS_LINUX)

scoped_refptr<VideoFrame> CreateVideoFrameGpu(
    gpu::GpuMemoryBufferFactory* factory,
    VideoPixelFormat pixel_format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp,
    gfx::BufferUsage buffer_usage) {
  DCHECK(factory);
  auto buffer_format = VideoPixelFormatToGfxBufferFormat(pixel_format);
  if (!buffer_format)
    return nullptr;

  static base::AtomicSequenceNumber buffer_id_generator;
  auto gmb_handle = factory->CreateGpuMemoryBuffer(
      gfx::GpuMemoryBufferId(buffer_id_generator.GetNext()), coded_size,
      *buffer_format, buffer_usage, gpu::kPlatformVideoFramePoolClientId,
      gfx::kNullAcceleratedWidget);
  if (gmb_handle.is_null() || gmb_handle.type != gfx::NATIVE_PIXMAP)
    return nullptr;

  DCHECK_EQ(VideoFrame::NumPlanes(pixel_format),
            gmb_handle.native_pixmap_handle.planes.size());
  std::vector<ColorPlaneLayout> planes;
  for (const auto& plane : gmb_handle.native_pixmap_handle.planes)
    planes.emplace_back(plane.stride, plane.offset, plane.size);

  auto layout = VideoFrameLayout::CreateWithPlanes(
      pixel_format, coded_size, std::move(planes),
      VideoFrameLayout::kBufferAddressAlignment,
      gmb_handle.native_pixmap_handle.modifier);

  if (!layout)
    return nullptr;

  std::vector<base::ScopedFD> dmabuf_fds;
  for (const auto& plane : gmb_handle.native_pixmap_handle.planes) {
    int duped_fd = HANDLE_EINTR(dup(plane.fd.get()));
    if (duped_fd == -1) {
      DLOG(ERROR) << "Failed duplicating dmabuf fd";
      return nullptr;
    }
    dmabuf_fds.emplace_back(duped_fd);
  }

  auto frame = VideoFrame::WrapExternalDmabufs(
      *layout, visible_rect, visible_rect.size(), std::move(dmabuf_fds),
      timestamp);
  if (!frame)
    return nullptr;

  // Created |gmb_handle| must be owned by |frame|.
  frame->AddDestructionObserver(
      base::BindOnce(base::DoNothing::Once<gfx::GpuMemoryBufferHandle>(),
                     std::move(gmb_handle)));
  // We also need to have the factory drop its reference to the native pixmap.
  frame->AddDestructionObserver(
      base::BindOnce(&gpu::GpuMemoryBufferFactory::DestroyGpuMemoryBuffer,
                     base::Unretained(factory), gmb_handle.id,
                     gpu::kPlatformVideoFramePoolClientId));
  return frame;
}
#endif  // defined(OS_LINUX)

}  // namespace

scoped_refptr<VideoFrame> CreatePlatformVideoFrame(
    gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory,
    VideoPixelFormat pixel_format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp,
    gfx::BufferUsage buffer_usage) {
#if defined(OS_LINUX)
  return CreateVideoFrameGpu(gpu_memory_buffer_factory, pixel_format,
                             coded_size, visible_rect, natural_size, timestamp,
                             buffer_usage);
#endif  // defined(OS_LINUX)
  NOTREACHED();
  return nullptr;
}

base::Optional<VideoFrameLayout> GetPlatformVideoFrameLayout(
    gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory,
    VideoPixelFormat pixel_format,
    const gfx::Size& coded_size,
    gfx::BufferUsage buffer_usage) {
  // |visible_rect| and |natural_size| do not matter here. |coded_size| is set
  // as a dummy variable.
  auto frame = CreatePlatformVideoFrame(
      gpu_memory_buffer_factory, pixel_format, coded_size,
      gfx::Rect(coded_size), coded_size, base::TimeDelta(), buffer_usage);
  return frame ? base::make_optional<VideoFrameLayout>(frame->layout())
               : base::nullopt;
}

gfx::GpuMemoryBufferHandle CreateGpuMemoryBufferHandle(
    const VideoFrame* video_frame) {
  DCHECK(video_frame);

  gfx::GpuMemoryBufferHandle handle;
  switch (video_frame->storage_type()) {
    case VideoFrame::STORAGE_GPU_MEMORY_BUFFER:
      handle = video_frame->GetGpuMemoryBuffer()->CloneHandle();
      break;
    case VideoFrame::STORAGE_DMABUFS: {
      handle.type = gfx::NATIVE_PIXMAP;
      std::vector<base::ScopedFD> duped_fds =
          DuplicateFDs(video_frame->DmabufFds());
      const size_t num_planes = VideoFrame::NumPlanes(video_frame->format());
      DCHECK_EQ(video_frame->layout().planes().size(), num_planes);
      handle.native_pixmap_handle.modifier = video_frame->layout().modifier();
      for (size_t i = 0; i < num_planes; ++i) {
        const auto& plane = video_frame->layout().planes()[i];
        handle.native_pixmap_handle.planes.emplace_back(
            plane.stride, plane.offset, plane.size, std::move(duped_fds[i]));
      }
    } break;
    default:
      NOTREACHED() << "Unsupported storage type: "
                   << video_frame->storage_type();
  }
  if (!handle.is_null() && handle.type == gfx::NATIVE_PIXMAP &&
      !VerifyGpuMemoryBufferHandle(video_frame->format(),
                                   video_frame->coded_size(), handle)) {
    VLOGF(1) << "Created GpuMemoryBufferHandle is invalid";
  }
  return handle;
}

scoped_refptr<gfx::NativePixmapDmaBuf> CreateNativePixmapDmaBuf(
    const VideoFrame* video_frame) {
  DCHECK(video_frame);

  // Create a native pixmap from the frame's memory buffer handle.
  gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle =
      CreateGpuMemoryBufferHandle(video_frame);
  if (gpu_memory_buffer_handle.is_null() ||
      gpu_memory_buffer_handle.type != gfx::NATIVE_PIXMAP) {
    VLOGF(1) << "Failed to create native GpuMemoryBufferHandle";
    return nullptr;
  }

  auto buffer_format =
      VideoPixelFormatToGfxBufferFormat(video_frame->layout().format());
  if (!buffer_format) {
    VLOGF(1) << "Unexpected video frame format";
    return nullptr;
  }

  auto native_pixmap = base::MakeRefCounted<gfx::NativePixmapDmaBuf>(
      video_frame->coded_size(), *buffer_format,
      std::move(gpu_memory_buffer_handle.native_pixmap_handle));

  DCHECK(native_pixmap->AreDmaBufFdsValid());
  return native_pixmap;
}

}  // namespace media
