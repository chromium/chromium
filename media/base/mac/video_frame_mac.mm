// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mac/video_frame_mac.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <array>
#include <vector>

#include "base/apple/bridging.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "media/base/mac/color_space_util_mac.h"
#include "media/base/media_switches.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"
#include "ui/gfx/hdr_metadata.h"
#include "ui/gfx/hdr_metadata_mac.h"

namespace media {

namespace {

// Maximum number of planes supported by this implementation.
const int kMaxPlanes = 3;

enum class CVPixelFormatStorage {
  kUncompressed,
  kCompressed,
};

struct VideoFrameCVPixelFormatInfo {
  VideoPixelFormat video_format;
  OSType cv_pixel_format;
  gfx::ColorSpace::RangeID range;
  CVPixelFormatStorage storage = CVPixelFormatStorage::kUncompressed;
};

constexpr auto kVideoFrameCVPixelFormatInfos =
    std::to_array<VideoFrameCVPixelFormatInfo>({
        {
            PIXEL_FORMAT_I420,
            kCVPixelFormatType_420YpCbCr8Planar,
            gfx::ColorSpace::RangeID::LIMITED,
        },
        {
            PIXEL_FORMAT_I420,
            kCVPixelFormatType_420YpCbCr8PlanarFullRange,
            gfx::ColorSpace::RangeID::FULL,
        },
        {
            PIXEL_FORMAT_NV12,
            kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange,
            gfx::ColorSpace::RangeID::LIMITED,
        },
        {
            PIXEL_FORMAT_NV12,
            kCVPixelFormatType_420YpCbCr8BiPlanarFullRange,
            gfx::ColorSpace::RangeID::FULL,
        },
        {
            PIXEL_FORMAT_NV12,
            kCVPixelFormatType_Lossless_420YpCbCr8BiPlanarVideoRange,
            gfx::ColorSpace::RangeID::LIMITED,
            CVPixelFormatStorage::kCompressed,
        },
        {
            PIXEL_FORMAT_NV12,
            kCVPixelFormatType_Lossless_420YpCbCr8BiPlanarFullRange,
            gfx::ColorSpace::RangeID::FULL,
            CVPixelFormatStorage::kCompressed,
        },
        {
            PIXEL_FORMAT_NV12A,
            kCVPixelFormatType_420YpCbCr8VideoRange_8A_TriPlanar,
            gfx::ColorSpace::RangeID::LIMITED,
        },
        {
            PIXEL_FORMAT_NV16,
            kCVPixelFormatType_422YpCbCr8BiPlanarVideoRange,
            gfx::ColorSpace::RangeID::LIMITED,
        },
        {
            PIXEL_FORMAT_NV16,
            kCVPixelFormatType_422YpCbCr8BiPlanarFullRange,
            gfx::ColorSpace::RangeID::FULL,
        },
        {
            PIXEL_FORMAT_NV24,
            kCVPixelFormatType_444YpCbCr8BiPlanarVideoRange,
            gfx::ColorSpace::RangeID::LIMITED,
        },
        {
            PIXEL_FORMAT_NV24,
            kCVPixelFormatType_444YpCbCr8BiPlanarFullRange,
            gfx::ColorSpace::RangeID::FULL,
        },
        {
            PIXEL_FORMAT_P010LE,
            kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange,
            gfx::ColorSpace::RangeID::LIMITED,
        },
        {
            PIXEL_FORMAT_P010LE,
            kCVPixelFormatType_420YpCbCr10BiPlanarFullRange,
            gfx::ColorSpace::RangeID::FULL,
        },
        {
            PIXEL_FORMAT_P010LE,
            kCVPixelFormatType_Lossless_420YpCbCr10PackedBiPlanarVideoRange,
            gfx::ColorSpace::RangeID::LIMITED,
            CVPixelFormatStorage::kCompressed,
        },
        {
            PIXEL_FORMAT_P010LE,
            kCVPixelFormatType_Lossless_420YpCbCr10PackedBiPlanarFullRange,
            gfx::ColorSpace::RangeID::FULL,
            CVPixelFormatStorage::kCompressed,
        },
        {
            PIXEL_FORMAT_P210LE,
            kCVPixelFormatType_422YpCbCr10BiPlanarVideoRange,
            gfx::ColorSpace::RangeID::LIMITED,
        },
        {
            PIXEL_FORMAT_P210LE,
            kCVPixelFormatType_422YpCbCr10BiPlanarFullRange,
            gfx::ColorSpace::RangeID::FULL,
        },
        {
            PIXEL_FORMAT_P210LE,
            kCVPixelFormatType_Lossless_422YpCbCr10PackedBiPlanarVideoRange,
            gfx::ColorSpace::RangeID::LIMITED,
            CVPixelFormatStorage::kCompressed,
        },
        {
            PIXEL_FORMAT_P410LE,
            kCVPixelFormatType_444YpCbCr10BiPlanarVideoRange,
            gfx::ColorSpace::RangeID::LIMITED,
        },
        {
            PIXEL_FORMAT_P410LE,
            kCVPixelFormatType_444YpCbCr10BiPlanarFullRange,
            gfx::ColorSpace::RangeID::FULL,
        },
    });

// CVPixelBuffer release callback. See |GetCvPixelBufferRepresentation()|.
void CvPixelBufferReleaseCallback(void* frame_ref,
                                  const void* data,
                                  size_t size,
                                  size_t num_planes,
                                  const void* planes[]) {
  free(const_cast<void*>(data));
  reinterpret_cast<const VideoFrame*>(frame_ref)->Release();
}

bool CvPixelBufferHasColorSpace(CVPixelBufferRef pixel_buffer) {
  return CVBufferHasAttachment(pixel_buffer, kCVImageBufferColorPrimariesKey) &&
         CVBufferHasAttachment(pixel_buffer,
                               kCVImageBufferTransferFunctionKey) &&
         CVBufferHasAttachment(pixel_buffer, kCVImageBufferYCbCrMatrixKey);
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

void SetCvPixelBufferHdrMetadata(const gfx::HDRMetadata& hdr_metadata,
                                 CVPixelBufferRef pixel_buffer) {
  if (!hdr_metadata.IsValid()) {
    return;
  }
  if (hdr_metadata.HasMDCV()) {
    CVBufferSetAttachment(
        pixel_buffer, kCVImageBufferMasteringDisplayColorVolumeKey,
        gfx::GenerateMasteringDisplayColorVolume(hdr_metadata).get(),
        kCVAttachmentMode_ShouldPropagate);
  }
  // GenerateContentLightLevelInfo() returns null for a zeroed out CLLI.
  if (auto clli = gfx::GenerateContentLightLevelInfo(hdr_metadata)) {
    CVBufferSetAttachment(pixel_buffer, kCVImageBufferContentLightLevelInfoKey,
                          clli.get(), kCVAttachmentMode_ShouldPropagate);
  }
}

void SetCvPixelBufferAttachments(const VideoFrame& frame,
                                 CVPixelBufferRef pixel_buffer) {
  SetCvPixelBufferColorSpace(frame.ColorSpace(), pixel_buffer);
  SetCvPixelBufferHdrMetadata(frame.hdr_metadata(), pixel_buffer);
}

void ApplyCvPixelBufferCleanApertureIfNeeded(const VideoFrame& frame,
                                             CVPixelBufferRef pixel_buffer) {
  const auto& coded_size = frame.coded_size();
  const auto& visible_rect = frame.visible_rect();
  if (visible_rect == gfx::Rect(coded_size)) {
    return;
  }

  // Unlike our visible rect, the clean aperture offsets are relative to the
  // center of image. There's not a lot of documentation on this calculation,
  // but see crabby_avifCleanApertureBoxConvertCropRect() for another impl.
  double horizontal_offset =
      visible_rect.x() - (coded_size.width() - visible_rect.width()) / 2.0;
  double vertical_offset =
      visible_rect.y() - (coded_size.height() - visible_rect.height()) / 2.0;
  NSDictionary* clean_aperture = @{
    base::apple::CFToNSPtrCast(kCVImageBufferCleanApertureWidthKey) :
        @(visible_rect.width()),
    base::apple::CFToNSPtrCast(kCVImageBufferCleanApertureHeightKey) :
        @(visible_rect.height()),
    base::apple::CFToNSPtrCast(kCVImageBufferCleanApertureHorizontalOffsetKey) :
        @(horizontal_offset),
    base::apple::CFToNSPtrCast(kCVImageBufferCleanApertureVerticalOffsetKey) :
        @(vertical_offset)
  };
  CVBufferSetAttachment(pixel_buffer, kCVImageBufferCleanApertureKey,
                        base::apple::NSToCFPtrCast(clean_aperture),
                        kCVAttachmentMode_ShouldPropagate);
}

}  // namespace

std::optional<OSType> CVPixelFormatForVideoFrame(
    VideoPixelFormat format,
    gfx::ColorSpace::RangeID range) {
  range = range == gfx::ColorSpace::RangeID::FULL
              ? gfx::ColorSpace::RangeID::FULL
              : gfx::ColorSpace::RangeID::LIMITED;
  const auto found = std::ranges::find_if(
      kVideoFrameCVPixelFormatInfos, [format, range](const auto& info) {
        return info.video_format == format && info.range == range &&
               info.storage == CVPixelFormatStorage::kUncompressed;
      });
  if (found == kVideoFrameCVPixelFormatInfos.end()) {
    return std::nullopt;
  }
  return found->cv_pixel_format;
}

bool IsAcceptableCvPixelFormat(VideoPixelFormat format,
                               OSType cv_pixel_format) {
  return std::ranges::any_of(kVideoFrameCVPixelFormatInfos,
                             [format, cv_pixel_format](const auto& info) {
                               return info.video_format == format &&
                                      info.cv_pixel_format == cv_pixel_format;
                             });
}

base::apple::ScopedCFTypeRef<CVPixelBufferRef> WrapIOSurfaceInCVPixelBuffer(
    const VideoFrame& frame,
    IOSurfaceRef io_surface) {
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> pixel_buffer;
  CVReturn cv_return = CVPixelBufferCreateWithIOSurface(
      nullptr, io_surface, nullptr, pixel_buffer.InitializeInto());
  if (cv_return != kCVReturnSuccess) {
    DLOG(ERROR) << "CVPixelBufferCreateWithIOSurface failed: " << cv_return;
    pixel_buffer.reset();
  } else if (!IsAcceptableCvPixelFormat(
                 frame.format(),
                 CVPixelBufferGetPixelFormatType(pixel_buffer.get()))) {
    DLOG(ERROR) << "Dropping CVPixelBuffer w/ incorrect format.";
    pixel_buffer.reset();
  } else {
    SetCvPixelBufferAttachments(frame, pixel_buffer.get());
    ApplyCvPixelBufferCleanApertureIfNeeded(frame, pixel_buffer.get());
  }
  return pixel_buffer;
}

base::apple::ScopedCFTypeRef<CVPixelBufferRef> WrapVideoFrameInCVPixelBuffer(
    scoped_refptr<VideoFrame> frame) {
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> pixel_buffer;
  if (!frame) {
    return pixel_buffer;
  }

  const auto& coded_size = frame->coded_size();
  const bool crop_needed = frame->visible_rect() != gfx::Rect(coded_size);
  if (!crop_needed || base::FeatureList::IsEnabled(
                          kVTVideoEncodeAcceleratorOpaqueSharedImageEncode)) {
    // If the frame has a mappable SharedImage, yank out its IOSurface if it
    // exists.
    if (frame->HasMappableSharedImage()) {
      auto handle = frame->GetGpuMemoryBufferHandle();
      if (handle.type == gfx::GpuMemoryBufferType::IO_SURFACE_BUFFER) {
        CHECK(handle.io_surface());
        return WrapIOSurfaceInCVPixelBuffer(*frame, handle.io_surface().get());
      }
    }
  }

  // If the mappable SharedImage could not be wrapped above, map it and handle
  // it like a software frame. There is no memcpy here.
  if (frame->HasMappableSharedImage()) {
    frame = ConvertToMemoryMappedFrame(std::move(frame));
  }
  if (!frame) {
    return pixel_buffer;
  }

  // VideoFrame only supports YUV formats and most of them are 'YVU' ordered,
  // which CVPixelBuffer does not support. This means we effectively can only
  // represent I420 and the biplanar NV12/NV16/NV24 and P010/P210/P410 family.
  // In addition, VideoFrame does not carry colorimetric information, so this
  // function assumes standard video range and ITU Rec 709 primaries.
  const VideoPixelFormat video_frame_format = frame->format();
  auto range = frame->ColorSpace().GetRangeID();
  if (video_frame_format == PIXEL_FORMAT_NV12A &&
      range == gfx::ColorSpace::RangeID::FULL) {
    DVLOG(1) << "Full range NV12A is not supported by the OS.";
    range = gfx::ColorSpace::RangeID::LIMITED;
  }
  const std::optional<OSType> cv_format =
      CVPixelFormatForVideoFrame(video_frame_format, range);
  if (!cv_format) {
    DLOG(ERROR) << "Unsupported frame format: " << video_frame_format;
    return pixel_buffer;
  }

  DCHECK(IsAcceptableCvPixelFormat(video_frame_format, cv_format.value()));

  int num_planes = VideoFrame::NumPlanes(video_frame_format);
  DCHECK_LE(num_planes, kMaxPlanes);

  // Build arrays for each plane's data pointer, dimensions and byte alignment.
  std::vector<void*> plane_ptrs(num_planes);
  std::vector<size_t> plane_widths(num_planes);
  std::vector<size_t> plane_heights(num_planes);
  std::vector<size_t> plane_bytes_per_row(num_planes);
  for (int plane_i = 0; plane_i < num_planes; ++plane_i) {
    plane_ptrs[plane_i] = const_cast<uint8_t*>(frame->data(plane_i));
    plane_widths[plane_i] = frame->columns(plane_i);
    plane_heights[plane_i] = frame->rows(plane_i);
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
      kCFAllocatorDefault, coded_size.width(), coded_size.height(),
      cv_format.value(), descriptor, 0, num_planes, plane_ptrs.data(),
      plane_widths.data(), plane_heights.data(), plane_bytes_per_row.data(),
      &CvPixelBufferReleaseCallback, frame.get(), nullptr,
      pixel_buffer.InitializeInto());
  if (result != kCVReturnSuccess) {
    DLOG(ERROR) << " CVPixelBufferCreateWithPlanarBytes failed: " << result;
    return base::apple::ScopedCFTypeRef<CVPixelBufferRef>(nullptr);
  }

  // We must guarantee that every row of the CVPixelBuffer has the full stride,
  // so we can't directly pass visible_data() pointers in. We must instead pass
  // the full coded data along with the crop rect.
  ApplyCvPixelBufferCleanApertureIfNeeded(*frame, pixel_buffer.get());

  // The CVPixelBuffer now references the data of the frame, so increment its
  // reference count manually. The release callback set on the pixel buffer will
  // release the frame.
  frame->AddRef();
  SetCvPixelBufferAttachments(*frame, pixel_buffer.get());
  return pixel_buffer;
}

}  // namespace media
