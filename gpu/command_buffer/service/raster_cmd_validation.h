// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains various validation functions for the Raster service.

#ifndef GPU_COMMAND_BUFFER_SERVICE_RASTER_CMD_VALIDATION_H_
#define GPU_COMMAND_BUFFER_SERVICE_RASTER_CMD_VALIDATION_H_

#include "gpu/command_buffer/common/raster_cmd_format.h"
#include "gpu/command_buffer/service/value_validator.h"

namespace gpu {
namespace raster {

struct Validators {
  Validators();

#include "gpu/command_buffer/service/raster_cmd_validation_autogen.h"
};

}  // namespace raster
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_RASTER_CMD_VALIDATION_H_
