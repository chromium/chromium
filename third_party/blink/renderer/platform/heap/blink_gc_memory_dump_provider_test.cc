// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/blink_gc_memory_dump_provider.h"

#include "base/trace_event/process_memory_dump.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/heap/blink_gc.h"
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

}  // namespace

TEST_F(BlinkGCMemoryDumpProviderTest, MainThreadLightDump) {
  base::trace_event::MemoryDumpArgs args = {
      base::trace_event::MemoryDumpLevelOfDetail::LIGHT};
  std::unique_ptr<base::trace_event::ProcessMemoryDump> dump(
      new base::trace_event::ProcessMemoryDump(args));
  std::unique_ptr<BlinkGCMemoryDumpProvider> dump_provider(
      new BlinkGCMemoryDumpProvider(
          ThreadState::Current(), base::ThreadTaskRunnerHandle::Get(),
          BlinkGCMemoryDumpProvider::HeapType::kBlinkMainThread));
  dump_provider->OnMemoryDump(args, dump.get());

  auto* main_heap = dump->GetAllocatorDump("blink_gc/main/heap");
  CheckBasicHeapDumpStructure(main_heap);
}

TEST_F(BlinkGCMemoryDumpProviderTest, MainThreadDetailedDump) {
  base::trace_event::MemoryDumpArgs args = {
      base::trace_event::MemoryDumpLevelOfDetail::DETAILED};
  std::unique_ptr<base::trace_event::ProcessMemoryDump> dump(
      new base::trace_event::ProcessMemoryDump(args));
  std::unique_ptr<BlinkGCMemoryDumpProvider> dump_provider(
      new BlinkGCMemoryDumpProvider(
          ThreadState::Current(), base::ThreadTaskRunnerHandle::Get(),
          BlinkGCMemoryDumpProvider::HeapType::kBlinkMainThread));
  dump_provider->OnMemoryDump(args, dump.get());

  // All arenas should be present in the dump.
#define CheckArena(name)       \
  CheckBasicHeapDumpStructure( \
      dump->GetAllocatorDump("blink_gc/main/heap/" #name "Arena"));

  FOR_EACH_ARENA(CheckArena)
#undef CheckArena
}

TEST_F(BlinkGCMemoryDumpProviderTest, WorkerLightDump) {
  base::trace_event::MemoryDumpArgs args = {
      base::trace_event::MemoryDumpLevelOfDetail::LIGHT};
  std::unique_ptr<base::trace_event::ProcessMemoryDump> dump(
      new base::trace_event::ProcessMemoryDump(args));
  std::unique_ptr<BlinkGCMemoryDumpProvider> dump_provider(
      new BlinkGCMemoryDumpProvider(
          ThreadState::Current(), base::ThreadTaskRunnerHandle::Get(),
          BlinkGCMemoryDumpProvider::HeapType::kBlinkWorkerThread));
  dump_provider->OnMemoryDump(args, dump.get());

  // There should be no main thread heap dump available.
  ASSERT_EQ(nullptr, dump->GetAllocatorDump("blink_gc/main/heap"));

  size_t workers_found = 0;
  for (const auto& kvp : dump->allocator_dumps()) {
    if (kvp.first.find("blink_gc/workers/heap") != std::string::npos) {
      workers_found++;
      CheckBasicHeapDumpStructure(dump->GetAllocatorDump(kvp.first));
    }
  }
  EXPECT_EQ(1u, workers_found);
}

TEST_F(BlinkGCMemoryDumpProviderTest, WorkerDetailedDump) {
  base::trace_event::MemoryDumpArgs args = {
      base::trace_event::MemoryDumpLevelOfDetail::DETAILED};
  std::unique_ptr<base::trace_event::ProcessMemoryDump> dump(
      new base::trace_event::ProcessMemoryDump(args));
  std::unique_ptr<BlinkGCMemoryDumpProvider> dump_provider(
      new BlinkGCMemoryDumpProvider(
          ThreadState::Current(), base::ThreadTaskRunnerHandle::Get(),
          BlinkGCMemoryDumpProvider::HeapType::kBlinkWorkerThread));
  dump_provider->OnMemoryDump(args, dump.get());

  // There should be no main thread heap dump available.
  ASSERT_EQ(nullptr, dump->GetAllocatorDump("blink_gc/main/heap"));

  // Find worker suffix.
  std::string worker_suffix;
  for (const auto& kvp : dump->allocator_dumps()) {
    if (kvp.first.find("blink_gc/workers/heap/worker_0x") !=
        std::string::npos) {
      auto start_pos = kvp.first.find("_0x");
      auto end_pos = kvp.first.find("/", start_pos);
      worker_suffix = kvp.first.substr(start_pos + 1, end_pos - start_pos - 1);
    }
  }
  std::string worker_base_path =
      "blink_gc/workers/heap/worker_" + worker_suffix;
  CheckBasicHeapDumpStructure(dump->GetAllocatorDump(worker_base_path));

#define CheckArena(name)       \
  CheckBasicHeapDumpStructure( \
      dump->GetAllocatorDump(worker_base_path + "/" #name "Arena"));

  FOR_EACH_ARENA(CheckArena)
#undef CheckArena
}

}  // namespace blink
