// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/renderer_process_security_state.h"

#include <atomic>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/check_is_test.h"
#include "base/memory/protected_memory.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/no_destructor.h"
#include "third_party/blink/public/common/renderer_process_security_state.h"

namespace blink {

namespace {

// Holds the pointer used for capability reads. Initialize() points it into the
// mapping owned by GetReadOnlyMapping(), then seals this page read-only so
// ordinary memory corruption cannot redirect those reads.
//
// A dedicated one-shot page is used instead of base::ProtectedMemory because
// this pointer is written once and never needs to become writable again.
// ProtectedMemory supports reopening values for writes and shares protection
// state with other protected values in the same DSO, which would unnecessarily
// couple this pointer's protection to unrelated values.
constinit base::subtle::OneShotProtectedPage<const std::atomic<uint32_t>*>
    g_security_state_capabilities;

using SecurityStateMemory = RendererProcessSecurityStateMemory;

// Owns the mapping for the process lifetime. The protected pointer page stores
// a pointer into this mapping, but does not own or extend its lifetime.
std::optional<SecurityStateMemory::ReadOnlyMapping>& GetReadOnlyMapping() {
  static base::NoDestructor<std::optional<SecurityStateMemory::ReadOnlyMapping>>
      mapping;
  return *mapping;
}

std::optional<SecurityStateMemory>& GetSecurityStateForTesting() {
  static base::NoDestructor<std::optional<SecurityStateMemory>> state;
  return *state;
}

bool IsInitialized() {
  return g_security_state_capabilities.value() != nullptr;
}

}  // namespace

// static
void RendererProcessSecurityState::Initialize(
    base::ReadOnlySharedMemoryRegion region) {
  CHECK(!IsInitialized());
  auto mapping = SecurityStateMemory::MapReadOnlyRegion(std::move(region));
  CHECK(mapping);
  GetReadOnlyMapping() = std::move(*mapping);
  g_security_state_capabilities.WriteOnceAndSealReadOnly(
      GetReadOnlyMapping()->ReadOnlyPtr());
}

// static
bool RendererProcessSecurityState::InitializeForTesting() {
  CHECK_IS_TEST();
  if (IsInitialized()) {
    return false;
  }

  auto state = SecurityStateMemory::Create(uint32_t{0});
  CHECK(state);
  base::ReadOnlySharedMemoryRegion read_only_region =
      state->TakeReadOnlyRegion();
  GetSecurityStateForTesting() = std::move(*state);
  Initialize(std::move(read_only_region));
  return true;
}

// static
bool RendererProcessSecurityState::HasCapability(
    RendererProcessCapability capability) {
  CHECK(IsInitialized());
  CHECK(IsValidRendererProcessCapability(capability));
  return (g_security_state_capabilities.value()->load(
              std::memory_order_acquire) &
          RendererProcessCapabilityMask(capability)) != 0;
}

// static
void RendererProcessSecurityState::SetCapabilitiesForTesting(
    uint32_t capabilities) {
  CHECK_IS_TEST();
  CHECK(GetSecurityStateForTesting());
  GetSecurityStateForTesting()->WritableRef().store(capabilities,
                                                    std::memory_order_release);
}

}  // namespace blink
