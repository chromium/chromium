// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/instrumentation/tracing/web_memory_allocator_dump.h"

#include "base/trace_event/memory_allocator_dump.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace blink {

WebMemoryAllocatorDump::WebMemoryAllocatorDump(
    base::trace_event::MemoryAllocatorDump* memory_allocator_dump)
    : memory_allocator_dump_(memory_allocator_dump),
      guid_(memory_allocator_dump->guid().ToUint64()) {}

void WebMemoryAllocatorDump::AddScalar(const char* name,
                                       const char* units,
                                       uint64_t value) {
  memory_allocator_dump_->AddScalar(name, units, value);
}

void WebMemoryAllocatorDump::AddString(const char* name,
                                       const char* units,
                                       const String& value) {
  memory_allocator_dump_->AddString(name, units, value.Utf8());
}

WebMemoryAllocatorDumpGuid WebMemoryAllocatorDump::Guid() const {
  return guid_;
}

}  // namespace blink
