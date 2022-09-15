// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_SKIA_MEMORY_DUMP_PROVIDER_H_
#define SKIA_EXT_SKIA_MEMORY_DUMP_PROVIDER_H_

#include "base/memory/singleton.h"
#include "base/trace_event/memory_dump_provider.h"
#include "third_party/skia/include/core/SkTypes.h"

namespace skia {

class SK_API SkiaMemoryDumpProvider
    : public base::trace_event::MemoryDumpProvider {
 public:
  static SkiaMemoryDumpProvider* GetInstance();

  SkiaMemoryDumpProvider(const SkiaMemoryDumpProvider&) = delete;
  SkiaMemoryDumpProvider& operator=(const SkiaMemoryDumpProvider&) = delete;

  // base::trace_event::MemoryDumpProvider implementation:
  bool OnMemoryDump(
      const base::trace_event::MemoryDumpArgs& args,
      base::trace_event::ProcessMemoryDump* process_memory_dump) override;

 private:
  friend struct base::DefaultSingletonTraits<SkiaMemoryDumpProvider>;

  SkiaMemoryDumpProvider();
  ~SkiaMemoryDumpProvider() override;
};

}  // namespace skia

#endif  // SKIA_EXT_SKIA_MEMORY_DUMP_PROVIDER_H_
