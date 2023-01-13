// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_SKDISCARDABLEMEMORY_CHROME_H_
#define SKIA_EXT_SKDISCARDABLEMEMORY_CHROME_H_

#include <memory>

#include "third_party/skia/include/private/chromium/SkDiscardableMemory.h"

namespace base {
class DiscardableMemory;

namespace trace_event {
class MemoryAllocatorDump;
class ProcessMemoryDump;
}

}  // namespace base

// This class implements the SkDiscardableMemory interface using
// base::DiscardableMemory.
class SK_API SkDiscardableMemoryChrome : public SkDiscardableMemory {
public:
 ~SkDiscardableMemoryChrome() override;

  // SkDiscardableMemory:
 bool lock() override;
 void* data() override;
 void unlock() override;

 base::trace_event::MemoryAllocatorDump* CreateMemoryAllocatorDump(
     const char* name,
     base::trace_event::ProcessMemoryDump* pmd) const;

private:
  friend class SkDiscardableMemory;

  SkDiscardableMemoryChrome(std::unique_ptr<base::DiscardableMemory> memory);

  std::unique_ptr<base::DiscardableMemory> discardable_;
};

#endif  // SKIA_EXT_SKDISCARDABLEMEMORY_CHROME_H_
