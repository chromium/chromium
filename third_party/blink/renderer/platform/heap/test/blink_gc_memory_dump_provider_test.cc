// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/blink_gc_memory_dump_provider.h"

#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "base/trace_event/process_memory_dump.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/platform/heap/custom_spaces.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"

namespace blink {

namespace {
class BlinkGCMemoryDumpProviderTest : public TestSupportingGC {};

void CheckBasicHeapDumpStructure(base::trace_event::MemoryAllocatorDump* dump) {
  ASSERT_NE(nullptr, dump);

  bool found_allocated_object_size = false;
  bool found_size = false;
  for (const auto& entry : dump->entries()) {
    if (entry.name == "allocated_objects_size")
      found_allocated_object_size = true;
    if (entry.name == "size")
      found_size = true;
  }
  EXPECT_TRUE(found_allocated_object_size);
  EXPECT_TRUE(found_size);
}

template <typename Callback>
void IterateMemoryDumps(base::trace_event::ProcessMemoryDump& dump,
                        const std::string dump_prefix,
                        Callback callback) {
  auto dump_prefix_depth = base::ranges::count(dump_prefix, '/');
  for (auto& it : dump.allocator_dumps()) {
    const std::string& key = it.first;
    if ((key.compare(0, dump_prefix.size(), dump_prefix) == 0) &&
        (base::ranges::count(key, '/') == dump_prefix_depth)) {
      callback(it.second.get());
    }
  }
}

void CheckSpacesInDump(base::trace_event::ProcessMemoryDump& dump,
                       const std::string dump_prefix) {
  size_t custom_space_count = 0;
  IterateMemoryDumps(
      dump, dump_prefix + "CustomSpace",
      [&custom_space_count](base::trace_event::MemoryAllocatorDump*) {
        custom_space_count++;
      });
  EXPECT_EQ(CustomSpaces::CreateCustomSpaces().size(), custom_space_count);
}

}  // namespace

TEST_F(BlinkGCMemoryDumpProviderTest, MainThreadLightDump) {
  base::trace_event::MemoryDumpArgs args = {
      base::trace_event::MemoryDumpLevelOfDetail::kLight};
  std::unique_ptr<base::trace_event::ProcessMemoryDump> dump(
      new base::trace_event::ProcessMemoryDump(args));
  std::unique_ptr<BlinkGCMemoryDumpProvider> dump_provider(
      new BlinkGCMemoryDumpProvider(
          ThreadState::Current(),
          scheduler::GetSingleThreadTaskRunnerForTesting(),
          BlinkGCMemoryDumpProvider::HeapType::kBlinkMainThread));
  dump_provider->OnMemoryDump(args, dump.get());

  auto* main_heap = dump->GetAllocatorDump("blink_gc/main/heap");
  CheckBasicHeapDumpStructure(main_heap);
}

TEST_F(BlinkGCMemoryDumpProviderTest, MainThreadDetailedDump) {
  base::trace_event::MemoryDumpArgs args = {
      base::trace_event::MemoryDumpLevelOfDetail::kDetailed};
  std::unique_ptr<base::trace_event::ProcessMemoryDump> dump(
      new base::trace_event::ProcessMemoryDump(args));
  std::unique_ptr<BlinkGCMemoryDumpProvider> dump_provider(
      new BlinkGCMemoryDumpProvider(
          ThreadState::Current(),
          scheduler::GetSingleThreadTaskRunnerForTesting(),
          BlinkGCMemoryDumpProvider::HeapType::kBlinkMainThread));
  dump_provider->OnMemoryDump(args, dump.get());

  IterateMemoryDumps(*dump, "blink_gc/main/heap/", CheckBasicHeapDumpStructure);
  CheckSpacesInDump(*dump, "blink_gc/main/heap/");
}

TEST_F(BlinkGCMemoryDumpProviderTest, WorkerLightDump) {
  base::trace_event::MemoryDumpArgs args = {
      base::trace_event::MemoryDumpLevelOfDetail::kLight};
  std::unique_ptr<base::trace_event::ProcessMemoryDump> dump(
      new base::trace_event::ProcessMemoryDump(args));
  std::unique_ptr<BlinkGCMemoryDumpProvider> dump_provider(
      new BlinkGCMemoryDumpProvider(
          ThreadState::Current(),
          scheduler::GetSingleThreadTaskRunnerForTesting(),
          BlinkGCMemoryDumpProvider::HeapType::kBlinkWorkerThread));
  dump_provider->OnMemoryDump(args, dump.get());

  // There should be no main thread heap dump available.
  ASSERT_EQ(nullptr, dump->GetAllocatorDump("blink_gc/main/heap"));

  size_t workers_found = 0;
  for (const auto& kvp : dump->allocator_dumps()) {
    if (base::Contains(kvp.first, "blink_gc/workers/")) {
      workers_found++;
      CheckBasicHeapDumpStructure(dump->GetAllocatorDump(kvp.first));
    }
  }
  EXPECT_EQ(1u, workers_found);
}

TEST_F(BlinkGCMemoryDumpProviderTest, WorkerDetailedDump) {
  base::trace_event::MemoryDumpArgs args = {
      base::trace_event::MemoryDumpLevelOfDetail::kDetailed};
  std::unique_ptr<base::trace_event::ProcessMemoryDump> dump(
      new base::trace_event::ProcessMemoryDump(args));
  std::unique_ptr<BlinkGCMemoryDumpProvider> dump_provider(
      new BlinkGCMemoryDumpProvider(
          ThreadState::Current(),
          scheduler::GetSingleThreadTaskRunnerForTesting(),
          BlinkGCMemoryDumpProvider::HeapType::kBlinkWorkerThread));
  dump_provider->OnMemoryDump(args, dump.get());

  const std::string worker_path_prefix = "blink_gc/workers";
  const std::string worker_path_suffix = "/heap";

  // Find worker suffix.
  std::string worker_suffix;
  for (const auto& kvp : dump->allocator_dumps()) {
    if (base::Contains(kvp.first, worker_path_prefix + "/worker_0x")) {
      auto start_pos = kvp.first.find("_0x");
      auto end_pos = kvp.first.find("/", start_pos);
      worker_suffix = kvp.first.substr(start_pos + 1, end_pos - start_pos - 1);
    }
  }
  std::string worker_base_path =
      worker_path_prefix + "/worker_" + worker_suffix + worker_path_suffix;
  CheckBasicHeapDumpStructure(dump->GetAllocatorDump(worker_base_path));

  IterateMemoryDumps(*dump, worker_base_path + "/",
                     CheckBasicHeapDumpStructure);
  CheckSpacesInDump(*dump, worker_base_path + "/");
}

}  // namespace blink
