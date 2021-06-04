// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the binary format definition of the command buffer and
// command buffer commands.

// We explicitly do NOT include raster_cmd_format.h here because client side
// and service side have different requirements.
#include "gpu/command_buffer/common/cmd_buffer_common.h"

#include <stddef.h>

#include "base/cxx17_backports.h"

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
  return (index < base::size(names)) ? names[index] : "*unknown-command*";
}

}  // namespace raster
}  // namespace gpu
