// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains various validation functions for the GLES2 service.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_VALIDATION_H_
#define GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_VALIDATION_H_

#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/service/value_validator.h"

namespace gpu {
namespace gles2 {

struct Validators {
  Validators();

  void UpdateValuesES3();
  void UpdateETCCompressedTextureFormats();

#include "gpu/command_buffer/service/gles2_cmd_validation_autogen.h"
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_VALIDATION_H_

