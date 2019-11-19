// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_V8_SHARED_MEMORY_DUMP_PROVIDER_H_
#define GIN_V8_SHARED_MEMORY_DUMP_PROVIDER_H_

#include <string>

#include "base/macros.h"
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

  // MemoryDumpProvider implementation.
  bool OnMemoryDump(
      const base::trace_event::MemoryDumpArgs& args,
      base::trace_event::ProcessMemoryDump* process_memory_dump) override;

  static void Register();

 private:
  DISALLOW_COPY_AND_ASSIGN(V8SharedMemoryDumpProvider);
};

}  // namespace gin

#endif  // GIN_V8_SHARED_MEMORY_DUMP_PROVIDER_H_
