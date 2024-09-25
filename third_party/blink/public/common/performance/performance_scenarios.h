// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PERFORMANCE_PERFORMANCE_SCENARIOS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PERFORMANCE_PERFORMANCE_SCENARIOS_H_

#include <atomic>
#include <utility>

#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/shared_memory_safety_checker.h"
#include "base/memory/structured_shared_memory.h"
#include "third_party/blink/public/common/common_export.h"

namespace blink::performance_scenarios {

// Defines performance scenarios that a page can be in.
//
// Each enum is a list of mutually-exclusive scenarios. The complete scenario
// state is a tuple of all scenarios that are detected, at most one from each
// enum.
//
// The browser process detects which scenarios apply and shares that state with
// child processes over shared memory. Each process can view a global scenario
// list over the entire browser (eg. some page is loading) or a scenario list
// targeted only to that process (eg. a page hosted in this process is loading).

// Scenarios indicating a page is loading.
enum class LoadingScenario {
  // No pages covered by the scenario are loading.
  kNoPageLoading = 0,
  // The focused page is loading. Implies the page is also visible.
  kFocusedPageLoading,
  // The focused page (if any) is not loading, but a visible page is loading.
  kVisiblePageLoading,
  // No visible pages are loading, but a non-visible page is.
  kBackgroundPageLoading,
};

// Scenarios indicating user input.
enum class InputScenario {
  // TODO(crbug.com/365586676): Add additional scenarios.
  kNoInput = 0,
};

// The scope that a scenario covers.
enum class Scope {
  // The scenario covers only pages hosted in the current process.
  kCurrentProcess,
  // The scenario covers the whole browser.
  kGlobal,
};

// The full scenario state to copy over shared memory.
#pragma clang diagnostic push
#pragma clang diagnostic error "-Wpadded"
struct ScenarioState {
  base::subtle::SharedAtomic<LoadingScenario> loading;
  base::subtle::SharedAtomic<InputScenario> input;
};
#pragma clang diagnostic pop

using SharedScenarioState = base::StructuredSharedMemory<ScenarioState>;

// Pointers to the mapped shared memory are held in thread-safe scoped_refptr's.
// The memory will be unmapped when the final reference is dropped. Functions
// that copy values out of the shared memory must hold a reference to it so that
// it's not unmapped while reading.
using RefCountedScenarioMapping =
    base::RefCountedData<SharedScenarioState::ReadOnlyMapping>;

// A wrapper around a std::atomic<T> that's stored in shared memory. The wrapper
// prevents the shared memory from being unmapped while a caller has a reference
// to the atomic. Dereference the SharedAtomicRef to read from it as a
// std::atomic. See the comments above GetLoadingScenario() for usage notes.
template <typename T>
class SharedAtomicRef {
 public:
  SharedAtomicRef(scoped_refptr<RefCountedScenarioMapping> mapping,
                  const std::atomic<T>& wrapped_atomic)
      : mapping_(std::move(mapping)), wrapped_atomic_(wrapped_atomic) {}

  ~SharedAtomicRef() = default;

  // Move-only.
  SharedAtomicRef(const SharedAtomicRef&) = delete;
  SharedAtomicRef& operator=(const SharedAtomicRef&) = delete;
  SharedAtomicRef(SharedAtomicRef&&) = default;
  SharedAtomicRef& operator=(SharedAtomicRef&&) = default;

  // Smart-pointer-like interface:

  // Returns a pointer to the wrapped atomic.
  const std::atomic<T>* get() const { return &wrapped_atomic_; }

  // Returns a reference to the wrapped atomic.
  const std::atomic<T>& operator*() const { return wrapped_atomic_; }

  // Returns a pointer to the wrapped atomic for method invocation.
  const std::atomic<T>* operator->() const { return &wrapped_atomic_; }

 private:
  const scoped_refptr<RefCountedScenarioMapping> mapping_;

  // A reference into `mapping_`, not PartitionAlloc memory.
  RAW_PTR_EXCLUSION const std::atomic<T>& wrapped_atomic_;
};

// A scoped object that maps shared memory for the scenario state into the
// current process as long as it exists.
class BLINK_COMMON_EXPORT ScopedReadOnlyScenarioMemory {
 public:
  // Maps `region` into the current process, as a read-only view of the memory
  // holding the scenario state for `scope`.
  ScopedReadOnlyScenarioMemory(Scope scope,
                               base::ReadOnlySharedMemoryRegion region);
  ~ScopedReadOnlyScenarioMemory();

  ScopedReadOnlyScenarioMemory(const ScopedReadOnlyScenarioMemory&) = delete;
  ScopedReadOnlyScenarioMemory& operator=(const ScopedReadOnlyScenarioMemory&) =
      delete;

  // Returns a pointer to the mapping registered for `scope`, if any.
  static scoped_refptr<RefCountedScenarioMapping> GetMappingForTesting(
      Scope scope);

 private:
  Scope scope_;
};

// Functions to query performance scenarios.
//
// Since the scenarios can be modified at any time from another process, they're
// accessed through SharedAtomicRef. Get a snapshot of the scenario with
// std::atomic::load(). std::memory_order_relaxed is usually sufficient since no
// other memory depends on the scenario value.
//
// Usage:
//
//   // Test whether any foreground page is loading.
//   LoadingScenario scenario = GetLoadingScenario(Scope::kGlobal)->load(
//                                  std::memory_order_relaxed);
//   if (scenario == LoadingScenario::kFocusedPageLoading ||
//       scenario == LoadingScenario::kVisiblePageLoading) {
//     ... delay less-important work until scenario changes ...
//   }
//
//   // Test whether the current process is in the critical path for user input.
//   if (GetInputScenario(Scope::kCurrentProcess)->load(
//           std::memory_order_relaxed) != InputScenario::kNoInput) {
//     ... current process should prioritize input responsiveness ...
//   }

// Returns a reference to the loading scenario for `scope`.
BLINK_COMMON_EXPORT SharedAtomicRef<LoadingScenario> GetLoadingScenario(
    Scope scope);

// Returns a reference to the input scenario for `scope`.
BLINK_COMMON_EXPORT SharedAtomicRef<InputScenario> GetInputScenario(
    Scope scope);

}  // namespace blink::performance_scenarios

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PERFORMANCE_PERFORMANCE_SCENARIOS_H_
