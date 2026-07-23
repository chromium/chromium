// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/dawn_wire_server.h"

#include <memory>
#include <span>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "gpu/config/gpu_finch_features.h"

namespace gpu::webgpu {

std::unique_ptr<DawnWireServer> DawnWireServer::Create(
    dawn::wire::CommandSerializer* serializer,
    dawn::wire::server::MemoryTransferService* memory_transfer_service,
    const DawnProcTable& procs) {
  dawn::wire::WireServerDescriptor descriptor = {};
  descriptor.procs = &procs;
  descriptor.serializer = serializer;
  descriptor.memoryTransferService = memory_transfer_service;
  descriptor.useSpontaneousCallbacks =
      features::kWebGPUSpontaneousWireServer.Get();

  return base::WrapUnique(new DawnWireServer(descriptor));
}

bool DawnWireServer::HandleCommands(
    base::span<const volatile uint8_t> commands) {
  // SAFETY: Casting of uint8_t to std::byte is well-defined.
  return HandleCommands(UNSAFE_BUFFERS(std::span<const volatile std::byte>(
      reinterpret_cast<const volatile std::byte*>(commands.data()),
      commands.size())));
}

}  // namespace gpu::webgpu
