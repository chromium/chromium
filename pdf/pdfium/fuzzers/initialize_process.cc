// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/memory.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "testing/libfuzzer/libfuzzer_exports.h"

namespace {
base::TestDiscardableMemoryAllocator g_discardable_memory_allocator;
}

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv) {
  base::EnableTerminationOnOutOfMemory();
  base::DiscardableMemoryAllocator::SetInstance(
      &g_discardable_memory_allocator);
  return 0;
}
