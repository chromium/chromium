// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/dawn_wire_client.h"

#include <span>

#include "base/compiler_specific.h"
#include "base/containers/span.h"

namespace gpu::webgpu {

bool DawnWireClient::HandleCommands(
    base::span<const volatile uint8_t> commands) {
  // SAFETY: Casting of uint8_t to std::byte is well-defined.
  return HandleCommands(UNSAFE_BUFFERS(std::span<const volatile std::byte>(
      reinterpret_cast<const volatile std::byte*>(commands.data()),
      commands.size())));
}

}  // namespace gpu::webgpu
