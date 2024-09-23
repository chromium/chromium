// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// This file contains the binary format definition of the command buffer and
// command buffer commands.

#include "gpu/command_buffer/common/cmd_buffer_common.h"

#include <stdint.h>

#include "gpu/command_buffer/common/command_buffer.h"

namespace gpu {
#if !defined(_WIN32)
// gcc needs this to link, but MSVC requires it not be present
const int32_t CommandHeader::kMaxSize;
#endif
namespace cmd {

const char* GetCommandName(CommandId command_id) {
  static const char* const names[] = {
  #define COMMON_COMMAND_BUFFER_CMD_OP(name) # name,

  COMMON_COMMAND_BUFFER_CMDS(COMMON_COMMAND_BUFFER_CMD_OP)

  #undef COMMON_COMMAND_BUFFER_CMD_OP
  };

  int id = static_cast<int>(command_id);
  return (id >= 0 && id < kNumCommands) ? names[id] : "*unknown-command*";
}

}  // namespace cmd
}  // namespace gpu


