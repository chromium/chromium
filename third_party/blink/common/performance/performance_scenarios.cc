// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/performance/performance_scenarios.h"

#include <optional>
#include <utility>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/structured_shared_memory.h"
#include "base/no_destructor.h"
#include "base/notreached.h"

namespace blink::performance_scenarios {

namespace {

// Global pointers to the shared memory mappings.
scoped_refptr<RefCountedScenarioMapping>& MappingPtrForScope(Scope scope) {
  static base::NoDestructor<scoped_refptr<RefCountedScenarioMapping>>
      current_process_mapping;
  static base::NoDestructor<scoped_refptr<RefCountedScenarioMapping>>
      global_mapping;
  switch (scope) {
    case Scope::kCurrentProcess:
      return *current_process_mapping;
    case Scope::kGlobal:
      return *global_mapping;
  }
  NOTREACHED();
}

// Returns the scenario state from `mapping`, or a default empty state if
// `mapping` is null (which can happen if no ScopedReadOnlyScenarioMemory exists
// or if the mapping failed). Takes a raw pointer instead of a scoped_ptr to
// avoid refcount churn.
const ScenarioState& GetScenarioStateFromMapping(
    const RefCountedScenarioMapping* mapping) {
  static constinit ScenarioState kDummyScenarioState;
  return mapping ? mapping->data.ReadOnlyRef() : kDummyScenarioState;
}

}  // namespace

// TODO(crbug.com/365586676): Currently these are only mapped into browser and
// renderer processes. The global scenarios should also be mapped into utility
// processes.

ScopedReadOnlyScenarioMemory::ScopedReadOnlyScenarioMemory(
    Scope scope,
    base::ReadOnlySharedMemoryRegion region)
    : scope_(scope) {
  std::optional<SharedScenarioState::ReadOnlyMapping> mapping =
      SharedScenarioState::MapReadOnlyRegion(std::move(region));
  if (mapping.has_value()) {
    MappingPtrForScope(scope_) =
        base::MakeRefCounted<RefCountedScenarioMapping>(
            std::move(mapping.value()));
  }
}

ScopedReadOnlyScenarioMemory::~ScopedReadOnlyScenarioMemory() {
  MappingPtrForScope(scope_).reset();
}

// static
scoped_refptr<RefCountedScenarioMapping>
ScopedReadOnlyScenarioMemory::GetMappingForTesting(Scope scope) {
  return MappingPtrForScope(scope);
}

SharedAtomicRef<LoadingScenario> GetLoadingScenario(Scope scope) {
  scoped_refptr<RefCountedScenarioMapping> mapping = MappingPtrForScope(scope);
  return SharedAtomicRef<LoadingScenario>(
      mapping, GetScenarioStateFromMapping(mapping.get()).loading);
}

SharedAtomicRef<InputScenario> GetInputScenario(Scope scope) {
  scoped_refptr<RefCountedScenarioMapping> mapping = MappingPtrForScope(scope);
  return SharedAtomicRef<InputScenario>(
      mapping, GetScenarioStateFromMapping(mapping.get()).input);
}

}  // namespace blink::performance_scenarios
