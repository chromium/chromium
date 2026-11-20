// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_RENDERER_PROCESS_SECURITY_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_RENDERER_PROCESS_SECURITY_STATE_H_

#include <stdint.h>

#include "third_party/blink/renderer/platform/platform_export.h"

namespace base {
class ReadOnlySharedMemoryRegion;
}  // namespace base

namespace blink {

enum class RendererProcessCapability : uint32_t;

// Provides read-only access to browser-controlled renderer process
// capabilities. The state is initialized exactly once before any capability
// queries and remains mapped for the lifetime of the renderer process.
// Initialize() stores the mapped state address in a dedicated page and
// permanently makes that page read-only.
//
// This is defense in depth against attacks limited to data corruption and
// constrained code reuse: the renderer cannot change capability bits through
// ordinary memory writes, overwrite the stored mapping address, or call a
// renderer-side API to grant itself a capability. It is not an in-process
// security boundary against unrestricted arbitrary code execution. An attacker
// with that level of control can modify the renderer's own address space,
// including changing page protections or replacing mappings. Authorization
// that must remain secure after full renderer compromise must also be enforced
// by the browser or another trusted process.
class PLATFORM_EXPORT RendererProcessSecurityState {
 public:
  // Maps a valid read-only duplicate of browser-owned security state. CHECKs
  // if the region is invalid or the process-wide state is already initialized.
  static void Initialize(base::ReadOnlySharedMemoryRegion region);

  // Initializes mutable browser-like backing for tests that do not run a
  // renderer initialized by the browser. Returns false without changing state
  // if normal or testing initialization already occurred.
  //
  // TODO(crbug.com/511765966): Remove this comment when the follow-up CL uses
  // this method for MojoJS tests.
  static bool InitializeForTesting();

  static bool HasCapability(RendererProcessCapability capability);

  // Requires successful InitializeForTesting(). CHECKs if normal browser
  // initialization occurred instead.
  static void SetCapabilitiesForTesting(uint32_t capabilities);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_RENDERER_PROCESS_SECURITY_STATE_H_
