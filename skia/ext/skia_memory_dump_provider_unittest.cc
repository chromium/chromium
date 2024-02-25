// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/skia_memory_dump_provider.h"

#include <memory>

#include "base/trace_event/process_memory_dump.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace skia {

// Tests if the skia dump provider dumps without crashing.
TEST(SkiaMemoryDumpProviderTest, OnMemoryDump) {
  base::trace_event::MemoryDumpArgs dump_args = {
      base::trace_event::MemoryDumpLevelOfDetail::kDetailed};
  std::unique_ptr<base::trace_event::ProcessMemoryDump> process_memory_dump(
      new base::trace_event::ProcessMemoryDump(dump_args));
  SkiaMemoryDumpProvider::GetInstance()->OnMemoryDump(
      dump_args, process_memory_dump.get());

  ASSERT_TRUE(process_memory_dump->GetAllocatorDump("skia/sk_glyph_cache"));
}

}  // namespace skia
