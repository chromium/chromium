// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/base/mac/video_frame_mac.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "media/base/mac/color_space_util_mac.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "ui/gfx/gpu_memory_buffer.h"

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

// Current list of acceptable CVPixelFormat mappings. If we start supporting
// RGB frame encoding we'll need to extend this list.
bool IsAcceptableCvPixelFormat(VideoPixelFormat format, OSType cv_format) {
  if (format == PIXEL_FORMAT_I420) {
    return cv_format == kCVPixelFormatType_420YpCbCr8Planar ||
           cv_format == kCVPixelFormatType_420YpCbCr8PlanarFullRange;
  }
  if (format == PIXEL_FORMAT_NV12) {
    return cv_format == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange ||
           cv_format == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange;
  }
  if (format == PIXEL_FORMAT_NV12A) {
    return cv_format == kCVPixelFormatType_420YpCbCr8VideoRange_8A_TriPlanar;
  }
  return false;
}

bool CvPixelBufferHasColorSpace(CVPixelBufferRef pixel_buffer) {
  if (@available(macOS 12, iOS 15, *)) {
    return CVBufferHasAttachment(pixel_buffer,
                                 kCVImageBufferColorPrimariesKey) &&
           CVBufferHasAttachment(pixel_buffer,
                                 kCVImageBufferTransferFunctionKey) &&
           CVBufferHasAttachment(pixel_buffer, kCVImageBufferYCbCrMatrixKey);
  } else {
#if !defined(__IPHONE_15_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_15_0
    return CVBufferGetAttachment(pixel_buffer, kCVImageBufferColorPrimariesKey,
                                 nullptr) &&
           CVBufferGetAttachment(pixel_buffer,
                                 kCVImageBufferTransferFunctionKey, nullptr) &&
           CVBufferGetAttachment(pixel_buffer, kCVImageBufferYCbCrMatrixKey,
                                 nullptr);
#else
    return false;
#endif
  }
}

void SetCvPixelBufferColorSpace(const gfx::ColorSpace& frame_cs,
                                CVPixelBufferRef pixel_buffer) {
  // Apply required colorimetric attachments.
  CFStringRef primary, transfer, matrix;
  if (frame_cs.IsValid() &&
      GetImageBufferColorValues(frame_cs, &primary, &transfer, &matrix)) {
    CVBufferSetAttachment(pixel_buffer, kCVImageBufferColorPrimariesKey,
                          primary, kCVAttachmentMode_ShouldPropagate);
    CVBufferSetAttachment(pixel_buffer, kCVImageBufferTransferFunctionKey,
                          transfer, kCVAttachmentMode_ShouldPropagate);
    CVBufferSetAttachment(pixel_buffer, kCVImageBufferYCbCrMatrixKey, matrix,
                          kCVAttachmentMode_ShouldPropagate);
  } else if (!CvPixelBufferHasColorSpace(pixel_buffer)) {
    CVBufferSetAttachment(pixel_buffer, kCVImageBufferColorPrimariesKey,
                          kCVImageBufferColorPrimaries_ITU_R_709_2,
                          kCVAttachmentMode_ShouldPropagate);
    CVBufferSetAttachment(pixel_buffer, kCVImageBufferTransferFunctionKey,
                          kCVImageBufferTransferFunction_ITU_R_709_2,
                          kCVAttachmentMode_ShouldPropagate);
    CVBufferSetAttachment(pixel_buffer, kCVImageBufferYCbCrMatrixKey,
                          kCVImageBufferYCbCrMatrix_ITU_R_709_2,
                          kCVAttachmentMode_ShouldPropagate);
  }
}

}  // namespace

MEDIA_EXPORT base::apple::ScopedCFTypeRef<CVPixelBufferRef>
WrapVideoFrameInCVPixelBuffer(scoped_refptr<VideoFrame> frame) {
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> pixel_buffer;
  if (!frame) {
    return pixel_buffer;
  }

  const gfx::Rect& visible_rect = frame->visible_rect();
  bool crop_needed = visible_rect != gfx::Rect(frame->coded_size());

  if (!crop_needed) {
    // If the frame is backed by a pixel buffer, just return that buffer.
    if (frame->CvPixelBuffer()) {
      pixel_buffer.reset(frame->CvPixelBuffer(), base::scoped_policy::RETAIN);
      if (!IsAcceptableCvPixelFormat(
              frame->format(),
              CVPixelBufferGetPixelFormatType(pixel_buffer.get()))) {
        DLOG(ERROR) << "Dropping CVPixelBuffer w/ incorrect format.";
        pixel_buffer.reset();
      } else {
        SetCvPixelBufferColorSpace(frame->ColorSpace(), pixel_buffer.get());
      }
      return pixel_buffer;
    }

    // If the frame has a GMB, yank out its IOSurface if possible.
    if (frame->HasMappableGpuBuffer()) {
      auto handle = frame->GetGpuMemoryBufferHandle();
      if (handle.type == gfx::GpuMemoryBufferType::IO_SURFACE_BUFFER) {
        gfx::ScopedIOSurface io_surface = handle.io_surface;
        if (io_surface) {
          CVReturn cv_return = CVPixelBufferCreateWithIOSurface(
              nullptr, io_surface.get(), nullptr,
              pixel_buffer.InitializeInto());
          if (cv_return != kCVReturnSuccess) {
            DLOG(ERROR) << "CVPixelBufferCreateWithIOSurface failed: "
                        << cv_return;
            pixel_buffer.reset();
          }
          if (!IsAcceptableCvPixelFormat(
                  frame->format(),
                  CVPixelBufferGetPixelFormatType(pixel_buffer.get()))) {
            DLOG(ERROR) << "Dropping CVPixelBuffer w/ incorrect format.";
            pixel_buffer.reset();
          } else {
            SetCvPixelBufferColorSpace(frame->ColorSpace(), pixel_buffer.get());
          }
          return pixel_buffer;
        }
      }
    }
  }

  // If the frame is backed by a GPU buffer, but needs cropping, map it and
  // and handle like a software frame. There is no memcpy here.
  if (frame->HasMappableGpuBuffer()) {
    frame = ConvertToMemoryMappedFrame(std::move(frame));
  }
  if (!frame) {
    return pixel_buffer;
  }

  // VideoFrame only supports YUV formats and most of them are 'YVU' ordered,
  // which CVPixelBuffer does not support. This means we effectively can only
  // represent I420 and NV12 frames. In addition, VideoFrame does not carry
  // colorimetric information, so this function assumes standard video range
  // and ITU Rec 709 primaries.
  const VideoPixelFormat video_frame_format = frame->format();
  const bool is_full_range =
      frame->ColorSpace().GetRangeID() == gfx::ColorSpace::RangeID::FULL;
  OSType cv_format;
  if (video_frame_format == PIXEL_FORMAT_I420) {
    cv_format = is_full_range ? kCVPixelFormatType_420YpCbCr8PlanarFullRange
                              : kCVPixelFormatType_420YpCbCr8Planar;
  } else if (video_frame_format == PIXEL_FORMAT_NV12) {
    cv_format = is_full_range ? kCVPixelFormatType_420YpCbCr8BiPlanarFullRange
                              : kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
  } else if (video_frame_format == PIXEL_FORMAT_NV12A) {
    if (is_full_range) {
      DVLOG(1) << "Full range NV12A is not supported by the OS.";
    }
    cv_format = kCVPixelFormatType_420YpCbCr8VideoRange_8A_TriPlanar;
  } else {
    DLOG(ERROR) << "Unsupported frame format: " << video_frame_format;
    return pixel_buffer;
  }

  DCHECK(IsAcceptableCvPixelFormat(video_frame_format, cv_format));

  int num_planes = VideoFrame::NumPlanes(video_frame_format);
  DCHECK_LE(num_planes, kMaxPlanes);

  // Build arrays for each plane's data pointer, dimensions and byte alignment.
  void* plane_ptrs[kMaxPlanes];
  size_t plane_widths[kMaxPlanes];
  size_t plane_heights[kMaxPlanes];
  size_t plane_bytes_per_row[kMaxPlanes];
  for (int plane_i = 0; plane_i < num_planes; ++plane_i) {
    plane_ptrs[plane_i] = const_cast<uint8_t*>(frame->visible_data(plane_i));
    gfx::Size plane_size =
        VideoFrame::PlaneSize(video_frame_format, plane_i, visible_rect.size());
    plane_widths[plane_i] = plane_size.width();
    plane_heights[plane_i] = plane_size.height();
    plane_bytes_per_row[plane_i] = frame->stride(plane_i);
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
      frame.get(), nullptr, pixel_buffer.InitializeInto());
  if (result != kCVReturnSuccess) {
    DLOG(ERROR) << " CVPixelBufferCreateWithPlanarBytes failed: " << result;
    return base::apple::ScopedCFTypeRef<CVPixelBufferRef>(nullptr);
  }

  // The CVPixelBuffer now references the data of the frame, so increment its
  // reference count manually. The release callback set on the pixel buffer will
  // release the frame.
  frame->AddRef();
  SetCvPixelBufferColorSpace(frame->ColorSpace(), pixel_buffer.get());
  return pixel_buffer;
}

MEDIA_EXPORT bool IOSurfaceIsWebGPUCompatible(IOSurfaceRef io_surface) {
  switch (IOSurfaceGetPixelFormat(io_surface)) {
    case kCVPixelFormatType_64RGBAHalf:
    case kCVPixelFormatType_TwoComponent16Half:
    case kCVPixelFormatType_OneComponent16Half:
    case kCVPixelFormatType_ARGB2101010LEPacked:
    case kCVPixelFormatType_32RGBA:
    case kCVPixelFormatType_32BGRA:
    case kCVPixelFormatType_TwoComponent8:
    case kCVPixelFormatType_OneComponent8:
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
      return true;
    default:
      return false;
  }
}

}  // namespace media
