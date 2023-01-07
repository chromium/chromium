// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_DATA_PIPE_CONTROL_MESSAGE_H_
#define MOJO_CORE_DATA_PIPE_CONTROL_MESSAGE_H_

#include <stdint.h>

#include "mojo/core/ports/port_ref.h"
#include "mojo/public/c/system/macros.h"

namespace mojo {
namespace core {

class NodeController;

enum DataPipeCommand : uint32_t {
  // Signal to the consumer that new data is available.
  DATA_WAS_WRITTEN,

  // Signal to the producer that data has been consumed.
  DATA_WAS_READ,
};

// Message header for messages sent over a data pipe control port.
struct MOJO_ALIGNAS(8) DataPipeControlMessage {
  DataPipeCommand command;
  uint32_t num_bytes;
};

void SendDataPipeControlMessage(NodeController* node_controller,
                                const ports::PortRef& port,
                                DataPipeCommand command,
                                uint32_t num_bytes);

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_DATA_PIPE_CONTROL_MESSAGE_H_
