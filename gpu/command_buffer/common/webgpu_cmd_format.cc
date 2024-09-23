// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// This file contains the binary format definition of the command buffer and
// command buffer commands.

// We explicitly do NOT include webgpu_cmd_format.h here because client side
// and service side have different requirements.
#include "gpu/command_buffer/common/cmd_buffer_common.h"

namespace gpu {
namespace webgpu {

#include "gpu/command_buffer/common/webgpu_cmd_ids_autogen.h"

const char* GetCommandName(CommandId id) {
  static const char* const names[] = {
#define WEBGPU_CMD_OP(name) "k" #name,

      WEBGPU_COMMAND_LIST(WEBGPU_CMD_OP)

#undef WEBGPU_CMD_OP
  };

  size_t index = static_cast<size_t>(id) - kFirstWebGPUCommand;
  return (index < std::size(names)) ? names[index] : "*unknown-command*";
}

}  // namespace webgpu
}  // namespace gpu
