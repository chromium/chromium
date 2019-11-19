// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/v8_shared_memory_dump_provider.h"

#include <memory>

#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "gin/test/v8_test.h"

namespace gin {

typedef V8Test V8SharedMemoryDumpProviderTest;

// Checks if the dump provider runs without crashing and dumps root objects.
TEST_F(V8SharedMemoryDumpProviderTest, DumpStatistics) {
  V8SharedMemoryDumpProvider provider;

  base::trace_event::MemoryDumpArgs dump_args = {
      base::trace_event::MemoryDumpLevelOfDetail::DETAILED};
  std::unique_ptr<base::trace_event::ProcessMemoryDump> process_memory_dump(
      new base::trace_event::ProcessMemoryDump(dump_args));
  provider.OnMemoryDump(dump_args, process_memory_dump.get());
  const base::trace_event::ProcessMemoryDump::AllocatorDumpsMap&
      allocator_dumps = process_memory_dump->allocator_dumps();

  bool did_dump_shared_memory_stats = false;
  bool did_dump_read_only_space = false;
  for (const auto& name_dump : allocator_dumps) {
    const std::string& name = name_dump.first;
    if (name.find("v8/shared") != std::string::npos) {
      did_dump_shared_memory_stats = true;
    }
    if (name.find("v8/shared/read_only_space") != std::string::npos) {
      did_dump_read_only_space = true;
    }
  }

  ASSERT_TRUE(did_dump_shared_memory_stats);
  ASSERT_TRUE(did_dump_read_only_space);
}

}  // namespace gin
