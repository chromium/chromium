// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/performance/performance_scenarios.h"

#include <atomic>
#include <optional>
#include <utility>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/structured_shared_memory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink::performance_scenarios {

namespace {

TEST(PerformanceScenariosTest, MappedScenarioState) {
  auto shared_memory = SharedScenarioState::Create();
  ASSERT_TRUE(shared_memory.has_value());

  // Before the shared memory is mapped in, GetLoadingScenario should return
  // default values.
  EXPECT_EQ(GetLoadingScenario(Scope::kCurrentProcess)
                ->load(std::memory_order_relaxed),
            LoadingScenario::kNoPageLoading);
  EXPECT_EQ(GetLoadingScenario(Scope::kGlobal)->load(std::memory_order_relaxed),
            LoadingScenario::kNoPageLoading);

  {
    // Map the shared memory as the global state.
    ScopedReadOnlyScenarioMemory mapped_global_memory(
        Scope::kGlobal, shared_memory->DuplicateReadOnlyRegion());
    EXPECT_EQ(
        GetLoadingScenario(Scope::kGlobal)->load(std::memory_order_relaxed),
        LoadingScenario::kNoPageLoading);

    // Updates should be visible in the global state only.
    shared_memory->WritableRef().loading.store(
        LoadingScenario::kFocusedPageLoading, std::memory_order_relaxed);
    EXPECT_EQ(
        GetLoadingScenario(Scope::kGlobal)->load(std::memory_order_relaxed),
        LoadingScenario::kFocusedPageLoading);
    EXPECT_EQ(GetLoadingScenario(Scope::kCurrentProcess)
                  ->load(std::memory_order_relaxed),
              LoadingScenario::kNoPageLoading);

    // Map the same shared memory as the per-process state.
    ScopedReadOnlyScenarioMemory mapped_current_memory(
        Scope::kCurrentProcess, shared_memory->DuplicateReadOnlyRegion());
    EXPECT_EQ(GetLoadingScenario(Scope::kCurrentProcess)
                  ->load(std::memory_order_relaxed),
              LoadingScenario::kFocusedPageLoading);

    // Updates should be visible in both mappings.
    shared_memory->WritableRef().loading.store(
        LoadingScenario::kVisiblePageLoading, std::memory_order_relaxed);
    EXPECT_EQ(
        GetLoadingScenario(Scope::kGlobal)->load(std::memory_order_relaxed),
        LoadingScenario::kVisiblePageLoading);
    EXPECT_EQ(GetLoadingScenario(Scope::kCurrentProcess)
                  ->load(std::memory_order_relaxed),
              LoadingScenario::kVisiblePageLoading);
  }

  // After going out of scope, the memory is unmapped and GetLoadingScenario
  // should see default values again.
  EXPECT_EQ(GetLoadingScenario(Scope::kCurrentProcess)
                ->load(std::memory_order_relaxed),
            LoadingScenario::kNoPageLoading);
  EXPECT_EQ(GetLoadingScenario(Scope::kGlobal)->load(std::memory_order_relaxed),
            LoadingScenario::kNoPageLoading);
}

TEST(PerformanceScenariosTest, SharedAtomicRef) {
  // Create and map shared memory.
  auto shared_memory = SharedScenarioState::Create();
  ASSERT_TRUE(shared_memory.has_value());
  auto read_only_mapping = SharedScenarioState::MapReadOnlyRegion(
      shared_memory->DuplicateReadOnlyRegion());
  ASSERT_TRUE(read_only_mapping.has_value());

  // Store pointers to the atomics in the shared memory for later comparison.
  const std::atomic<LoadingScenario>* loading_ptr =
      &(read_only_mapping->ReadOnlyRef().loading);
  const std::atomic<InputScenario>* input_ptr =
      &(read_only_mapping->ReadOnlyRef().input);

  // Transfer ownership of the mapping to a scoped_refptr.
  auto mapping_ptr = base::MakeRefCounted<RefCountedScenarioMapping>(
      std::move(read_only_mapping.value()));

  SharedAtomicRef<LoadingScenario> loading_ref(
      mapping_ptr, mapping_ptr->data.ReadOnlyRef().loading);
  SharedAtomicRef<InputScenario> input_ref(
      mapping_ptr, mapping_ptr->data.ReadOnlyRef().input);

  // The SharedAtomicRef's should keep the mapping alive.
  mapping_ptr.reset();
  shared_memory->WritableRef().loading.store(
      LoadingScenario::kBackgroundPageLoading, std::memory_order_relaxed);
  shared_memory->WritableRef().input.store(InputScenario::kNoInput,
                                           std::memory_order_relaxed);

  // get()
  EXPECT_EQ(loading_ref.get(), loading_ptr);
  EXPECT_EQ(input_ref.get(), input_ptr);

  // operator*
  EXPECT_EQ(*loading_ref, *loading_ptr);
  EXPECT_EQ(*input_ref, *input_ptr);

  // operator->
  EXPECT_EQ(loading_ref->load(std::memory_order_relaxed),
            LoadingScenario::kBackgroundPageLoading);
  EXPECT_EQ(input_ref->load(std::memory_order_relaxed),
            InputScenario::kNoInput);
}

}  // namespace

}  // namespace blink::performance_scenarios
