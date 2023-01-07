// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/gles2_cmd_copy_texture_chromium_utils.h"

#include "gpu/command_buffer/common/gles2_cmd_utils.h"

namespace gpu {
namespace gles2 {

bool CopyTextureCHROMIUMNeedsESSL3(uint32_t dest_format) {
  return gpu::gles2::GLES2Util::IsIntegerFormat(dest_format);
}

}  // namespace gles2
}  // namespace gpu
