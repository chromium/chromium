// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_HOST_GPU_MEMORY_BUFFER_SUPPORT_H_
#define GPU_IPC_HOST_GPU_MEMORY_BUFFER_SUPPORT_H_

#include <functional>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/hash/hash.h"
#include "ui/gfx/buffer_types.h"

namespace gpu {

// Returns true of the OpenGL target to use for the combination of format/usage
// is not GL_TEXTURE_2D but a platform specific texture target.
bool GetImageNeedsPlatformSpecificTextureTarget(gfx::BufferFormat format,
                                                gfx::BufferUsage usage);

// Populate a list of buffer usage/format for which a per platform specific
// texture target must be used instead of GL_TEXTURE_2D.
std::vector<gfx::BufferUsageAndFormat>
CreateBufferUsageAndFormatExceptionList();

}  // namespace gpu

#endif  // GPU_IPC_HOST_GPU_MEMORY_BUFFER_SUPPORT_H_
