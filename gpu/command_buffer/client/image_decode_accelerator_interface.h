// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_IMAGE_DECODE_ACCELERATOR_INTERFACE_H_
#define GPU_COMMAND_BUFFER_CLIENT_IMAGE_DECODE_ACCELERATOR_INTERFACE_H_

#include <stdint.h>

#include "base/containers/span.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/sync_token.h"

namespace cc {
struct ImageHeaderMetadata;
}

namespace gfx {
class ColorSpace;
class Size;
}  // namespace gfx

namespace gpu {

// TODO(andrescj): move API documentation from ImageDecodeAcceleratorProxy to
// here.
class ImageDecodeAcceleratorInterface {
 public:
  virtual ~ImageDecodeAcceleratorInterface() {}

  virtual bool IsImageSupported(
      const cc::ImageHeaderMetadata* image_metadata) const = 0;

  virtual bool IsJpegDecodeAccelerationSupported() const = 0;

  virtual bool IsWebPDecodeAccelerationSupported() const = 0;

  virtual SyncToken ScheduleImageDecode(
      base::span<const uint8_t> encoded_data,
      const gfx::Size& output_size,
      CommandBufferId raster_decoder_command_buffer_id,
      uint32_t transfer_cache_entry_id,
      int32_t discardable_handle_shm_id,
      uint32_t discardable_handle_shm_offset,
      uint64_t discardable_handle_release_count,
      const gfx::ColorSpace& target_color_space,
      bool needs_mips) = 0;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_IMAGE_DECODE_ACCELERATOR_INTERFACE_H_
