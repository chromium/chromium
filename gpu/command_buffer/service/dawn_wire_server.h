// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_DAWN_WIRE_SERVER_H_
#define GPU_COMMAND_BUFFER_SERVICE_DAWN_WIRE_SERVER_H_

#include <memory>

#include "base/containers/span.h"
#include "third_party/dawn/include/dawn/wire/WireServer.h"

namespace gpu::webgpu {

class DawnWireServer : public dawn::wire::WireServer {
 public:
  static std::unique_ptr<DawnWireServer> Create(
      dawn::wire::CommandSerializer* serializer,
      dawn::wire::server::MemoryTransferService* memory_transfer_service,
      const DawnProcTable& procs);

  bool HandleCommands(base::span<const volatile uint8_t> commands);

 private:
  // Hide the std::span versions of the API from the base class to prevent it
  // from being used directly in Chromium.
  using dawn::wire::WireServer::HandleCommands;

  using dawn::wire::WireServer::WireServer;
};

}  // namespace gpu::webgpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_DAWN_WIRE_SERVER_H_
