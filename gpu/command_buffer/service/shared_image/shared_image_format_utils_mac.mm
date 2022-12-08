// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/shared_image_format_utils.h"

#include <Metal/MTLPixelFormat.h>

#include "base/check_op.h"
#include "components/viz/common/resources/resource_format_utils.h"

namespace gpu {

unsigned int ToMTLPixelFormat(viz::SharedImageFormat format, int plane_index) {
  if (format.is_single_plane())
    return viz::ToMTLPixelFormat(format.resource_format());

  // Does not support external sampler.
  if (format.PrefersExternalSampler())
    return static_cast<unsigned int>(MTLPixelFormatInvalid);

  // For multiplanar formats without external sampler, Metal formats are per
  // plane.
  // For single channel 8-bit planes Y, U, V, A return MTLPixelFormatR8Unorm.
  // For 2 channel plane 8-bit UV return MTLPixelFormatRG8Unorm.
  // For single channel 10/16-bit planes Y, U, V, A return
  // MTLPixelFormatR16Unorm. For 2 channel plane 10/16-bit UV return
  // MTLPixelFormatRG16Unorm.
  int num_channels = format.NumChannelsInPlane(plane_index);
  DCHECK_LE(num_channels, 2);
  MTLPixelFormat mtl_pixel_format = MTLPixelFormatInvalid;
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