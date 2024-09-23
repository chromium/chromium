// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// This file contains the binary format definition of the command buffer and
// command buffer commands.

// We explicitly do NOT include raster_cmd_format.h here because client side
// and service side have different requirements.
#include <stddef.h>

#include "gpu/command_buffer/common/cmd_buffer_common.h"

namespace gpu {
namespace raster {

#include "gpu/command_buffer/common/raster_cmd_ids_autogen.h"

const char* GetCommandName(CommandId id) {
  static const char* const names[] = {
#define RASTER_CMD_OP(name) "k" #name,

      RASTER_COMMAND_LIST(RASTER_CMD_OP)

#undef RASTER_CMD_OP
  };

  size_t index = static_cast<size_t>(id) - kFirstRasterCommand;
  return (index < std::size(names)) ? names[index] : "*unknown-command*";
}

}  // namespace raster
}  // namespace gpu
