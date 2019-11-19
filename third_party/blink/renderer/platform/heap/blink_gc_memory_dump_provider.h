// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_BLINK_GC_MEMORY_DUMP_PROVIDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_BLINK_GC_MEMORY_DUMP_PROVIDER_H_

#include "base/trace_event/memory_dump_provider.h"
#include "third_party/blink/renderer/platform/heap/blink_gc.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace blink {

class ThreadState;

class PLATFORM_EXPORT BlinkGCMemoryDumpProvider final
    : public base::trace_event::MemoryDumpProvider {
  USING_FAST_MALLOC(BlinkGCMemoryDumpProvider);

 public:
  enum class HeapType { kBlinkMainThread, kBlinkWorkerThread };

  ~BlinkGCMemoryDumpProvider() final;
  BlinkGCMemoryDumpProvider(
      ThreadState* thread_state,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      HeapType heap_type);

  // MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs&,
                    base::trace_event::ProcessMemoryDump*) final;

 private:
  ThreadState* const thread_state_;
  const HeapType heap_type_;
  const std::string dump_base_name_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_BLINK_GC_MEMORY_DUMP_PROVIDER_H_
