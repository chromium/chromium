// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_GPU_MEMORY_BUFFER_SUPPORT_H_
#define GPU_COMMAND_BUFFER_COMMON_GPU_MEMORY_BUFFER_SUPPORT_H_

#include "base/containers/enum_set.h"
#include "build/build_config.h"
#include "gpu/gpu_export.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {

// A struct that represents a set of gfx::BufferFormat in a compact way.
using GpuMemoryBufferFormatSet = base::
    EnumSet<gfx::BufferFormat, gfx::BufferFormat::R_8, gfx::BufferFormat::LAST>;
static_assert(static_cast<int>(gfx::BufferFormat::R_8) == 0);
static_assert(static_cast<int>(gfx::BufferFormat::LAST) < 64);

struct Capabilities;

// Returns true if creating an image for a GpuMemoryBuffer with |format| is
// supported by |capabilities|.
GPU_EXPORT bool IsImageFromGpuMemoryBufferFormatSupported(
    gfx::BufferFormat format,
    const Capabilities& capabilities);

// Returns true if |size| is valid for plane |plane| of |format|.
GPU_EXPORT bool IsImageSizeValidForGpuMemoryBufferFormat(
    const gfx::Size& size,
    gfx::BufferFormat format);

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_GPU_MEMORY_BUFFER_SUPPORT_H_
