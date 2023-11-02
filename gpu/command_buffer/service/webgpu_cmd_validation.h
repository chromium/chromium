// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains various validation functions for the Webgpu service.

#ifndef GPU_COMMAND_BUFFER_SERVICE_WEBGPU_CMD_VALIDATION_H_
#define GPU_COMMAND_BUFFER_SERVICE_WEBGPU_CMD_VALIDATION_H_

#include "gpu/command_buffer/common/webgpu_cmd_enums.h"
#include "gpu/command_buffer/common/webgpu_cmd_format.h"
#include "gpu/command_buffer/service/value_validator.h"

namespace gpu {
namespace webgpu {

struct Validators {
  Validators();

#include "gpu/command_buffer/service/webgpu_cmd_validation_autogen.h"
};

}  // namespace webgpu
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_WEBGPU_CMD_VALIDATION_H_
