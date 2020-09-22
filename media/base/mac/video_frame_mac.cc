// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mac/video_frame_mac.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "base/logging.h"
#include "media/base/video_frame.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/mac/io_surface.h"

namespace media {

namespace {

// Maximum number of planes supported by this implementation.
const int kMaxPlanes = 3;

// CVPixelBuffer release callback. See |GetCvPixelBufferRepresentation()|.
void CvPixelBufferReleaseCallback(void* frame_ref,
                                  const void* data,
                                  size_t size,
                                  size_t num_planes,
                                  const void* planes[]) {
  free(const_cast<void*>(data));
  reinterpret_cast<const VideoFrame*>(frame_ref)->Release();
}

}  // namespace

MEDIA_EXPORT base::ScopedCFTypeRef<CVPixelBufferRef>
WrapVideoFrameInCVPixelBuffer(const VideoFrame& frame) {
  base::ScopedCFTypeRef<CVPixelBufferRef> pixel_buffer;

  // If the frame is backed by a pixel buffer, just return that buffer.
  if (frame.CvPixelBuffer()) {
    pixel_buffer.reset(frame.CvPixelBuffer(), base::scoped_policy::RETAIN);
    return pixel_buffer;
  }

  // If the frame has a GMB, yank out its IOSurface if possible.
  if (frame.GetGpuMemoryBuffer()) {
    gfx::GpuMemoryBufferHandle handle =
        frame.GetGpuMemoryBuffer()->CloneHandle();
    if (handle.type == gfx::GpuMemoryBufferType::IO_SURFACE_BUFFER) {
      base::ScopedCFTypeRef<IOSurfaceRef> io_surface =
          gfx::IOSurfaceMachPortToIOSurface(std::move(handle.mach_port));
      if (io_surface) {
        const CVReturn cv_return = CVPixelBufferCreateWithIOSurface(
            nullptr, io_surface, nullptr, pixel_buffer.InitializeInto());
        if (cv_return == kCVReturnSuccess) {
          VLOG(3) << "Returning IOSurface-based CVPixelBuffer.";
          return pixel_buffer;
        }
        pixel_buffer.reset();
      }
    }
  }

  VLOG(3) << "Returning RAM based CVPixelBuffer.";

  // VideoFrame only supports YUV formats and most of them are 'YVU' ordered,
  // which CVPixelBuffer does not support. This means we effectively can only
  // represent I420 and NV12 frames. In addition, VideoFrame does not carry
  // colorimetric information, so this function assumes standard video range
  // and ITU Rec 709 primaries.
  const VideoPixelFormat video_frame_format = frame.format();
  OSType cv_format;
  if (video_frame_format == PIXEL_FORMAT_I420) {
    cv_format = kCVPixelFormatType_420YpCbCr8Planar;
  } else if (video_frame_format == PIXEL_FORMAT_NV12) {
    cv_format = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
  } else {
    DLOG(ERROR) << " unsupported frame format: " << video_frame_format;
    return pixel_buffer;
  }

  int num_planes = VideoFrame::NumPlanes(video_frame_format);
  DCHECK_LE(num_planes, kMaxPlanes);
  const gfx::Rect& visible_rect = frame.visible_rect();

  // Build arrays for each plane's data pointer, dimensions and byte alignment.
  void* plane_ptrs[kMaxPlanes];
  size_t plane_widths[kMaxPlanes];
  size_t plane_heights[kMaxPlanes];
  size_t plane_bytes_per_row[kMaxPlanes];
  for (int plane_i = 0; plane_i < num_planes; ++plane_i) {
    plane_ptrs[plane_i] = const_cast<uint8_t*>(frame.visible_data(plane_i));
    gfx::Size plane_size =
        VideoFrame::PlaneSize(video_frame_format, plane_i, visible_rect.size());
    plane_widths[plane_i] = plane_size.width();
    plane_heights[plane_i] = plane_size.height();
    plane_bytes_per_row[plane_i] = frame.stride(plane_i);
  }

  // CVPixelBufferCreateWithPlanarBytes needs a dummy plane descriptor or the
  // release callback will not execute. The descriptor is freed in the callback.
  void* descriptor =
      calloc(1, std::max(sizeof(CVPlanarPixelBufferInfo_YCbCrPlanar),
                         sizeof(CVPlanarPixelBufferInfo_YCbCrBiPlanar)));

  // Wrap the frame's data in a CVPixelBuffer. Because this is a C API, we can't
  // give it a smart pointer to the frame, so instead pass a raw pointer and
  // increment the frame's reference count manually.
  CVReturn result = CVPixelBufferCreateWithPlanarBytes(
      kCFAllocatorDefault, visible_rect.width(), visible_rect.height(),
      cv_format, descriptor, 0, num_planes, plane_ptrs, plane_widths,
      plane_heights, plane_bytes_per_row, &CvPixelBufferReleaseCallback,
      const_cast<VideoFrame*>(&frame), nullptr, pixel_buffer.InitializeInto());
  if (result != kCVReturnSuccess) {
    DLOG(ERROR) << " CVPixelBufferCreateWithPlanarBytes failed: " << result;
    return base::ScopedCFTypeRef<CVPixelBufferRef>(nullptr);
  }

  // The CVPixelBuffer now references the data of the frame, so increment its
  // reference count manually. The release callback set on the pixel buffer will
  // release the frame.
  frame.AddRef();

  // Apply required colorimetric attachments.
  CVBufferSetAttachment(pixel_buffer, kCVImageBufferColorPrimariesKey,
                        kCVImageBufferColorPrimaries_ITU_R_709_2,
                        kCVAttachmentMode_ShouldPropagate);
  CVBufferSetAttachment(pixel_buffer, kCVImageBufferTransferFunctionKey,
                        kCVImageBufferTransferFunction_ITU_R_709_2,
                        kCVAttachmentMode_ShouldPropagate);
  CVBufferSetAttachment(pixel_buffer, kCVImageBufferYCbCrMatrixKey,
                        kCVImageBufferYCbCrMatrix_ITU_R_709_2,
                        kCVAttachmentMode_ShouldPropagate);

  return pixel_buffer;
}

}  // namespace media
