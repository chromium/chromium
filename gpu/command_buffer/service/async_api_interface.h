// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the command parser class.

#ifndef GPU_COMMAND_BUFFER_SERVICE_ASYNC_API_INTERFACE_H_
#define GPU_COMMAND_BUFFER_SERVICE_ASYNC_API_INTERFACE_H_

#include <stddef.h>
#include <stdint.h>

#include <string_view>

#include "gpu/command_buffer/common/constants.h"
#include "gpu/gpu_export.h"

namespace gpu {

// This class defines the interface for an asynchronous API handler, that
// is responsible for de-multiplexing commands and their arguments.
class GPU_EXPORT AsyncAPIInterface {
 public:
  AsyncAPIInterface() = default;
  virtual ~AsyncAPIInterface() = default;

  virtual void BeginDecoding() = 0;
  virtual void EndDecoding() = 0;

  // Executes multiple commands.
  // Parameters:
  //    num_commands: maximum number of commands to execute from buffer.
  //    buffer: pointer to first command entry to process.
  //    num_entries: number of sequential command buffer entries in buffer.
  //    entries_processed: if not 0, is set to the number of entries processed.
  virtual error::Error DoCommands(unsigned int num_commands,
                                  const volatile void* buffer,
                                  int num_entries,
                                  int* entries_processed) = 0;

  virtual std::string_view GetLogPrefix() = 0;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_ASYNC_API_INTERFACE_H_
