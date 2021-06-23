// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_V8_ISOLATE_MEMORY_DUMP_PROVIDER_H_
#define GIN_V8_ISOLATE_MEMORY_DUMP_PROVIDER_H_

#include <string>

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/trace_event/memory_dump_provider.h"
#include "gin/gin_export.h"

namespace gin {

class IsolateHolder;

// Memory dump provider for the chrome://tracing infrastructure. It dumps
// summarized memory stats about the V8 Isolate.
class V8IsolateMemoryDumpProvider
    : public base::trace_event::MemoryDumpProvider {
 public:
  V8IsolateMemoryDumpProvider(
      IsolateHolder* isolate_holder,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~V8IsolateMemoryDumpProvider() override;

  // MemoryDumpProvider implementation.
  bool OnMemoryDump(
      const base::trace_event::MemoryDumpArgs& args,
      base::trace_event::ProcessMemoryDump* process_memory_dump) override;

 private:
  void DumpHeapStatistics(
      const base::trace_event::MemoryDumpArgs& args,
      base::trace_event::ProcessMemoryDump* process_memory_dump);

  IsolateHolder* isolate_holder_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(V8IsolateMemoryDumpProvider);
};

}  // namespace gin

#endif  // GIN_V8_ISOLATE_MEMORY_DUMP_PROVIDER_H_
