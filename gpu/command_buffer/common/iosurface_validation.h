// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_IOSURFACE_VALIDATION_H_
#define GPU_COMMAND_BUFFER_COMMON_IOSURFACE_VALIDATION_H_

#include <string>

#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/common/gpu_command_buffer_common_export.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/mac/io_surface.h"

namespace gpu {

// Return the expected four character code pixel format for an IOSurface with
// the specified format.
uint32_t GPU_COMMAND_BUFFER_COMMON_EXPORT
SharedImageFormatToIOSurfacePixelFormat(viz::SharedImageFormat format,
                                        bool override_rgba_to_bgra);

// Ensure that the IOSurface has the same size and pixel format as those
// specified by `size` and `format`. A malicious client could lie about
// this, which, if subsequently used to determine parameters for bounds
// checking, could result in an out-of-bounds memory access.
bool GPU_COMMAND_BUFFER_COMMON_EXPORT
ValidateIOSurface(const gfx::ScopedIOSurface& io_surface,
                  viz::SharedImageFormat format,
                  gfx::Size size,
                  std::string* out_error_str);

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_IOSURFACE_VALIDATION_H_
