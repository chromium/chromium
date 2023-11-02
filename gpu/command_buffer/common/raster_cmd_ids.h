// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the raster command buffer commands.

#ifndef GPU_COMMAND_BUFFER_COMMON_RASTER_CMD_IDS_H_
#define GPU_COMMAND_BUFFER_COMMON_RASTER_CMD_IDS_H_

#include "gpu/command_buffer/common/cmd_buffer_common.h"

namespace gpu {
namespace raster {

#include "gpu/command_buffer/common/raster_cmd_ids_autogen.h"

const char* GetCommandName(CommandId command_id);

}  // namespace raster
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_RASTER_CMD_IDS_H_
