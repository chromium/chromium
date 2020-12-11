// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_SK_TRACE_MEMORY_DUMP_CHROME_H_
#define SKIA_EXT_SK_TRACE_MEMORY_DUMP_CHROME_H_

#include <stdint.h>

#include <string>

#include "base/macros.h"
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

  ~SkiaTraceMemoryDumpImpl() override;

  // SkTraceMemoryDump implementation:
  void dumpNumericValue(const char* dumpName,
                        const char* valueName,
                        const char* units,
                        uint64_t value) override;
  void dumpStringValue(const char* dump_name,
                       const char* value_name,
                       const char* value) override;
  void setMemoryBacking(const char* dumpName,
                        const char* backingType,
                        const char* backingObjectId) override;
  void setDiscardableMemoryBacking(
      const char* dumpName,
      const SkDiscardableMemory& discardableMemoryObject) override;
  LevelOfDetail getRequestedDetails() const override;
  bool shouldDumpWrappedObjects() const override;

 protected:
  base::trace_event::ProcessMemoryDump* process_memory_dump() {
    return process_memory_dump_;
  }

 private:
  std::string dump_name_prefix_;

  base::trace_event::ProcessMemoryDump* process_memory_dump_;

  // Stores the level of detail for the current dump.
  LevelOfDetail request_level_;

  DISALLOW_COPY_AND_ASSIGN(SkiaTraceMemoryDumpImpl);
};

}  // namespace skia

#endif  // SKIA_EXT_SK_TRACE_MEMORY_DUMP_CHROME_H_
