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

namespace gpu {

unsigned int ToMTLPixelFormat(viz::SharedImageFormat format, int plane_index) {
  MTLPixelFormat mtl_pixel_format = MTLPixelFormatInvalid;
  if (format.is_single_plane()) {
    if (format == viz::SinglePlaneFormat::kR_8 ||
        format == viz::SinglePlaneFormat::kALPHA_8) {
      mtl_pixel_format = MTLPixelFormatR8Unorm;
    } else if (format == viz::SinglePlaneFormat::kRG_88) {
      mtl_pixel_format = MTLPixelFormatRG8Unorm;
    } else if (format == viz::SinglePlaneFormat::kRGBA_8888) {
      mtl_pixel_format = MTLPixelFormatRGBA8Unorm;
    } else if (format == viz::SinglePlaneFormat::kBGRA_8888) {
      mtl_pixel_format = MTLPixelFormatBGRA8Unorm;
    } else if (format == viz::SinglePlaneFormat::kRGBA_F16) {
      mtl_pixel_format = MTLPixelFormatRGBA16Float;
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

}  // namespace gpu
