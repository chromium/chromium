// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_RASTER_CMD_HELPER_H_
#define GPU_COMMAND_BUFFER_CLIENT_RASTER_CMD_HELPER_H_

#include <stdint.h>

#include "gpu/command_buffer/client/cmd_buffer_helper.h"
#include "gpu/command_buffer/common/raster_cmd_format.h"
#include "gpu/raster_export.h"

namespace gpu {
namespace raster {

// A class that helps write GL command buffers.
class RASTER_EXPORT RasterCmdHelper : public CommandBufferHelper {
 public:
  explicit RasterCmdHelper(CommandBuffer* command_buffer);

  RasterCmdHelper(const RasterCmdHelper&) = delete;
  RasterCmdHelper& operator=(const RasterCmdHelper&) = delete;

  ~RasterCmdHelper() override;

// Include the auto-generated part of this class. We split this because it
// means we can easily edit the non-auto generated parts right here in this
// file instead of having to edit some template or the code generator.
#include "gpu/command_buffer/client/raster_cmd_helper_autogen.h"
};

}  // namespace raster
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_RASTER_CMD_HELPER_H_
