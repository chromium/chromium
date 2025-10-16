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

namespace gpu {

// TODO(andrescj): move API documentation from ImageDecodeAcceleratorProxy to
// here.
class ImageDecodeAcceleratorInterface {
 public:
  virtual ~ImageDecodeAcceleratorInterface() {}

  virtual bool IsImageSupported(
      const cc::ImageHeaderMetadata* image_metadata) const = 0;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_IMAGE_DECODE_ACCELERATOR_INTERFACE_H_
