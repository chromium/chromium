// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"

#include "base/containers/contains.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/capabilities.h"

namespace gpu {

bool IsFormatSupportedForSIWithNativeBuffer(
    viz::SharedImageFormat format,
    const gpu::Capabilities& capabilities) {
  const gfx::BufferFormat buffer_format =
      viz::SinglePlaneSharedImageFormatToBufferFormat(format);
  return capabilities.gpu_memory_buffer_formats.Has(buffer_format);
}

}  // namespace gpu
