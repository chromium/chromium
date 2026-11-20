// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_RENDERER_PROCESS_SECURITY_STATE_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_RENDERER_PROCESS_SECURITY_STATE_H_

#include <stdint.h>

#include "base/memory/structured_shared_memory.h"

namespace blink {

// Browser-controlled capabilities granted to a renderer process. Zero means
// that no capabilities are granted. Grants apply to the whole process and are
// monotonic for the process lifetime. Bit assignments are part of the
// browser-renderer shared-memory ABI and must never be renumbered or reused, so
// browser and renderer binaries of different versions interpret them
// consistently.
enum class RendererProcessCapability : uint32_t {
  // TODO(crbug.com/511765966): Remove this comment when the follow-up CL
  // migrates MojoJS permission checks to these capabilities.
  // Authorizes MojoJS to be enabled for contexts within the process.
  kMojoJSAllowedForProcess = 1U << 0,

  // Authorizes the MojoJSTest runtime feature. The browser grants this only
  // when it intentionally enables MojoJS test functionality for the renderer.
  kMojoJSTestAllowedForProcess = 1U << 1,
};

constexpr bool IsValidRendererProcessCapability(
    RendererProcessCapability capability) {
  switch (capability) {
    case RendererProcessCapability::kMojoJSAllowedForProcess:
    case RendererProcessCapability::kMojoJSTestAllowedForProcess:
      return true;
  }
  return false;
}

constexpr uint32_t RendererProcessCapabilityMask(
    RendererProcessCapability capability) {
  return static_cast<uint32_t>(capability);
}

using RendererProcessSecurityStateMemory = base::AtomicSharedMemory<uint32_t>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_RENDERER_PROCESS_SECURITY_STATE_H_
