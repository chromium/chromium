// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_SKIA_TRACE_MEMORY_DUMP_IMPL_H_
#define SKIA_EXT_SKIA_TRACE_MEMORY_DUMP_IMPL_H_

#include <stdint.h>

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "third_party/skia/include/core/SkTraceMemoryDump.h"

namespace base {
namespace trace_event {
class ProcessMemoryDump;
}
}

namespace skia {

class SK_API SkiaTraceMemoryDumpImpl : public SkTraceMemoryDump {
 public:
  // This should never outlive the OnMemoryDump call since the
  // ProcessMemoryDump is valid only in that timeframe. Optional
  // |dump_name_prefix| argument specifies the prefix appended to the dump
  // name skia provides. By default it is taken as empty string.
  SkiaTraceMemoryDumpImpl(
      base::trace_event::MemoryDumpLevelOfDetail level_of_detail,
      base::trace_event::ProcessMemoryDump* process_memory_dump);

  SkiaTraceMemoryDumpImpl(
      const std::string& dump_name_prefix,
      base::trace_event::MemoryDumpLevelOfDetail level_of_detail,
      base::trace_event::ProcessMemoryDump* process_memory_dump);

  SkiaTraceMemoryDumpImpl(const SkiaTraceMemoryDumpImpl&) = delete;
  SkiaTraceMemoryDumpImpl& operator=(const SkiaTraceMemoryDumpImpl&) = delete;

  ~SkiaTraceMemoryDumpImpl() override;

  // SkTraceMemoryDump implementation:
  void dumpNumericValue(const char* dump_name,
                        const char* value_name,
                        const char* units,
                        uint64_t value) override;
  void dumpStringValue(const char* dump_name,
                       const char* value_name,
                       const char* value) override;
  void setMemoryBacking(const char* dump_name,
                        const char* backing_type,
                        const char* backing_object_id) override;
  void setDiscardableMemoryBacking(
      const char* dump_name,
      const SkDiscardableMemory& discardable_memory) override;
  LevelOfDetail getRequestedDetails() const override;
  bool shouldDumpWrappedObjects() const override;
  void dumpWrappedState(const char* dump_name, bool wrapped) override;
  bool shouldDumpUnbudgetedObjects() const override;
  void dumpBudgetedState(const char* dump_name, bool budgeted) override;

 protected:
  base::trace_event::ProcessMemoryDump* process_memory_dump() {
    return process_memory_dump_;
  }

 private:
  std::string dump_name_prefix_;

  raw_ptr<base::trace_event::ProcessMemoryDump> process_memory_dump_;

  // Stores the level of detail for the current dump.
  LevelOfDetail request_level_;
};

}  // namespace skia

#endif  // SKIA_EXT_SKIA_TRACE_MEMORY_DUMP_IMPL_H_
