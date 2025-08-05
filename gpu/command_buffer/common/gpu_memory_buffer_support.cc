// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"

#include "base/containers/contains.h"
#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {

bool IsFormatSupportedForSIWithNativeBuffer(
    viz::SharedImageFormat format,
    const gpu::Capabilities& capabilities) {
  const gfx::BufferFormat buffer_format =
      viz::SinglePlaneSharedImageFormatToBufferFormat(format);
  return capabilities.gpu_memory_buffer_formats.Has(buffer_format);
}

bool IsSharedImageSizeValid(const gfx::Size& size,
                            viz::SharedImageFormat format) {
  if (format.is_single_plane()) {
    return true;
  }

#if BUILDFLAG(IS_CHROMEOS)
  // Allow odd size for CrOS.
  // TODO(https://crbug.com/1208788, https://crbug.com/1224781): Merge this
  // with the path that uses gfx::IsOddHeightMultiPlanarBuffersAllowed.
  return true;
#else
  if (size.width() % 2 && !gfx::IsOddWidthMultiPlanarBuffersAllowed()) {
    return false;
  }
  if (size.height() % 2 && !gfx::IsOddHeightMultiPlanarBuffersAllowed()) {
    return false;
  }
  return true;
#endif  // BUILDFLAG(IS_CHROMEOS)
}

}  // namespace gpu
