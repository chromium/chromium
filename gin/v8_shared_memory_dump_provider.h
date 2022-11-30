// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_V8_SHARED_MEMORY_DUMP_PROVIDER_H_
#define GIN_V8_SHARED_MEMORY_DUMP_PROVIDER_H_

#include "base/trace_event/memory_dump_provider.h"
#include "gin/gin_export.h"

namespace gin {

// Memory dump provider for the chrome://tracing infrastructure. It dumps
// summarized memory stats about V8 Memory shared between Isolates in the same
// process.
class GIN_EXPORT V8SharedMemoryDumpProvider
    : public base::trace_event::MemoryDumpProvider {
 public:
  V8SharedMemoryDumpProvider();
  V8SharedMemoryDumpProvider(const V8SharedMemoryDumpProvider&) = delete;
  V8SharedMemoryDumpProvider& operator=(const V8SharedMemoryDumpProvider&) =
      delete;

  // MemoryDumpProvider implementation.
  bool OnMemoryDump(
      const base::trace_event::MemoryDumpArgs& args,
      base::trace_event::ProcessMemoryDump* process_memory_dump) override;

  static void Register();
};

}  // namespace gin

#endif  // GIN_V8_SHARED_MEMORY_DUMP_PROVIDER_H_
