// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"

#include <CoreVideo/CoreVideo.h>
#include <Metal/Metal.h>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "components/viz/common/resources/shared_image_format.h"

#if BUILDFLAG(SKIA_USE_METAL)
#include "third_party/skia/include/gpu/graphite/mtl/MtlGraphiteTypes.h"
#endif

namespace gpu {

uint32_t SharedImageFormatToIOSurfacePixelFormat(viz::SharedImageFormat format,
                                                 bool override_rgba_to_bgra) {
  if (format.is_single_plane()) {
    if (format == viz::SinglePlaneFormat::kR_8) {
      return kCVPixelFormatType_OneComponent8;
    } else if (format == viz::SinglePlaneFormat::kRG_88) {
      return kCVPixelFormatType_TwoComponent8;
    } else if (format == viz::SinglePlaneFormat::kR_16) {
      return kCVPixelFormatType_OneComponent16;
    } else if (format == viz::SinglePlaneFormat::kRG_1616) {
      return kCVPixelFormatType_TwoComponent16;
    } else if (format == viz::SinglePlaneFormat::kBGRA_1010102) {
      return kCVPixelFormatType_ARGB2101010LEPacked;
    } else if (format == viz::SinglePlaneFormat::kBGRA_8888 ||
               format == viz::SinglePlaneFormat::kBGRX_8888) {
      return kCVPixelFormatType_32BGRA;
    } else if (format == viz::SinglePlaneFormat::kRGBA_8888 ||
               format == viz::SinglePlaneFormat::kRGBX_8888) {
      return override_rgba_to_bgra ? kCVPixelFormatType_32BGRA
                                   : kCVPixelFormatType_32RGBA;
    } else if (format == viz::SinglePlaneFormat::kRGBA_F16) {
      return kCVPixelFormatType_64RGBAHalf;
    } else if (format == viz::SinglePlaneFormat::kBGR_565 ||
               format == viz::SinglePlaneFormat::kRGBA_4444 ||
               format == viz::SinglePlaneFormat::kRGBA_1010102) {
      // Technically RGBA_1010102 should be accepted as 'R10k', but
      // then it won't be supported by CGLTexImageIOSurface2D(), so
      // it's best to reject it here.
      return 0;
    }
  } else if (format.is_multi_plane()) {
    if (format == viz::MultiPlaneFormat::kNV12) {
      return kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
    } else if (format == viz::MultiPlaneFormat::kNV16) {
      return kCVPixelFormatType_422YpCbCr8BiPlanarVideoRange;
    } else if (format == viz::MultiPlaneFormat::kNV24) {
      return kCVPixelFormatType_444YpCbCr8BiPlanarVideoRange;
    } else if (format == viz::MultiPlaneFormat::kNV12A) {
      return kCVPixelFormatType_420YpCbCr8VideoRange_8A_TriPlanar;
    } else if (format == viz::MultiPlaneFormat::kP010) {
      return kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange;
    } else if (format == viz::MultiPlaneFormat::kP210) {
      return kCVPixelFormatType_422YpCbCr10BiPlanarVideoRange;
    } else if (format == viz::MultiPlaneFormat::kP410) {
      return kCVPixelFormatType_444YpCbCr10BiPlanarVideoRange;
    } else if (format == viz::MultiPlaneFormat::kI420) {
      return kCVPixelFormatType_420YpCbCr8Planar;
    } else if (format == viz::MultiPlaneFormat::kYV12) {
      return 0;
    }
  }
  NOTREACHED();
}

unsigned int ToMTLPixelFormat(viz::SharedImageFormat format, int plane_index) {
  MTLPixelFormat mtl_pixel_format = MTLPixelFormatInvalid;
  if (format.is_single_plane()) {
    if (format == viz::SinglePlaneFormat::kR_8 ||
        format == viz::SinglePlaneFormat::kALPHA_8 ||
        format == viz::SinglePlaneFormat::kLUMINANCE_8) {
      mtl_pixel_format = MTLPixelFormatR8Unorm;
    } else if (format == viz::SinglePlaneFormat::kRG_88) {
      mtl_pixel_format = MTLPixelFormatRG8Unorm;
    } else if (format == viz::SinglePlaneFormat::kRGBA_8888) {
      mtl_pixel_format = MTLPixelFormatRGBA8Unorm;
    } else if (format == viz::SinglePlaneFormat::kBGRA_8888) {
      mtl_pixel_format = MTLPixelFormatBGRA8Unorm;
    } else {
      DLOG(ERROR) << "Invalid Metal pixel format:" << format.ToString();
    }
    return static_cast<unsigned int>(mtl_pixel_format);
  }

  // Does not support external sampler.
  if (format.PrefersExternalSampler()) {
    return static_cast<unsigned int>(MTLPixelFormatInvalid);
  }

  // For multiplanar formats without external sampler, Metal formats are per
  // plane.
  // For 1 channel 8-bit planes Y, U, V, A return MTLPixelFormatR8Unorm.
  // For 2 channel 8-bit plane UV return MTLPixelFormatRG8Unorm.
  // For 1 channel 10/16-bit planes Y, U, V, A return MTLPixelFormatR16Unorm.
  // For 2 channel 10/16-bit plane UV return MTLPixelFormatRG16Unorm.
  // For 1 channel 16-bit float planes Y, U, V, A return MTLPixelFormatR16Float.
  // For 2 channel 16-bit float plane UV return MTLPixelFormatRG16Float.
  int num_channels = format.NumChannelsInPlane(plane_index);
  DCHECK_LE(num_channels, 2);
  switch (format.channel_format()) {
    case viz::SharedImageFormat::ChannelFormat::k8:
      mtl_pixel_format =
          num_channels == 2 ? MTLPixelFormatRG8Unorm : MTLPixelFormatR8Unorm;
      break;
    case viz::SharedImageFormat::ChannelFormat::k10:
    case viz::SharedImageFormat::ChannelFormat::k16:
      mtl_pixel_format =
          num_channels == 2 ? MTLPixelFormatRG16Unorm : MTLPixelFormatR16Unorm;
      break;
    case viz::SharedImageFormat::ChannelFormat::k16F:
      mtl_pixel_format =
          num_channels == 2 ? MTLPixelFormatRG16Float : MTLPixelFormatR16Float;
      break;
  }
  return static_cast<unsigned int>(mtl_pixel_format);
}

#if BUILDFLAG(SKIA_USE_METAL)
skgpu::graphite::TextureInfo GraphiteMetalTextureInfo(
    viz::SharedImageFormat format,
    int plane_index,
    bool is_yuv_plane,
    bool mipmapped) {
  MTLPixelFormat mtl_pixel_format =
      static_cast<MTLPixelFormat>(ToMTLPixelFormat(format, plane_index));
  CHECK_NE(mtl_pixel_format, MTLPixelFormatInvalid);
  // Must match CreateMetalTexture in iosurface_image_backing.mm.
  // TODO(sunnyps): Move constants to a common utility header.
  skgpu::graphite::MtlTextureInfo mtl_texture_info;
  mtl_texture_info.fSampleCount = 1;
  mtl_texture_info.fFormat = mtl_pixel_format;
  mtl_texture_info.fUsage = MTLTextureUsageShaderRead;
  if (format.is_single_plane() && !is_yuv_plane) {
    mtl_texture_info.fUsage |= MTLTextureUsageRenderTarget;
  }
#if BUILDFLAG(IS_IOS)
  mtl_texture_info.fStorageMode = MTLStorageModeShared;
#else
  mtl_texture_info.fStorageMode = MTLStorageModeManaged;
#endif
  mtl_texture_info.fMipmapped =
      mipmapped ? skgpu::Mipmapped::kYes : skgpu::Mipmapped::kNo;
  return skgpu::graphite::TextureInfos::MakeMetal(mtl_texture_info);
}
#endif

}  // namespace gpu
