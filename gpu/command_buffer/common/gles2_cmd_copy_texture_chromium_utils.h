// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_COPY_TEXTURE_CHROMIUM_UTILS_H_
#define GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_COPY_TEXTURE_CHROMIUM_UTILS_H_

#include <stdint.h>

#include "gpu/command_buffer/common/gles2_utils_export.h"

namespace gpu {
namespace gles2 {

bool GLES2_UTILS_EXPORT CopyTextureCHROMIUMNeedsESSL3(uint32_t dest_format);

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_COPY_TEXTURE_CHROMIUM_UTILS_H_
