// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/v8_wrapper/blink_gc_memory_dump_provider.h"

#include <inttypes.h>

#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_dump_manager.h"

namespace blink {
namespace {

constexpr const char* HeapTypeString(
    BlinkGCMemoryDumpProvider::HeapType heap_type) {
  switch (heap_type) {
    case BlinkGCMemoryDumpProvider::HeapType::kBlinkMainThread:
      return "main";
    case BlinkGCMemoryDumpProvider::HeapType::kBlinkWorkerThread:
      return "workers";
  }
}

}  // namespace

BlinkGCMemoryDumpProvider::BlinkGCMemoryDumpProvider(
    ThreadState* thread_state,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    BlinkGCMemoryDumpProvider::HeapType heap_type)
    : thread_state_(thread_state),
      heap_type_(heap_type),
      dump_base_name_(
          "blink_gc/" + std::string(HeapTypeString(heap_type_)) + "/heap" +
          (heap_type_ == HeapType::kBlinkWorkerThread
               ? "/" + base::StringPrintf(
                           "worker_0x%" PRIXPTR,
                           reinterpret_cast<uintptr_t>(thread_state_))
               : "")) {
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "BlinkGC", task_runner);
}

BlinkGCMemoryDumpProvider::~BlinkGCMemoryDumpProvider() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

bool BlinkGCMemoryDumpProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* process_memory_dump) {
  // TODO(chromium:1056170): Provide implementation.
  return false;
}

}  // namespace blink
