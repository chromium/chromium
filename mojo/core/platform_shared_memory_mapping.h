// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_PLATFORM_SHARED_MEMORY_MAPPING_H_
#define MOJO_CORE_PLATFORM_SHARED_MEMORY_MAPPING_H_

#include <stddef.h>

#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "mojo/core/system_impl_export.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace mojo {
namespace core {

// A mapping of a |base::subtle::PlatformSharedMemoryRegion|, created
// exclusively by |SharedBufferDispatcher::MapBuffer()|. Automatically maps
// upon construction and unmaps upon destruction.
//
// Mappings are NOT thread-safe.
//
// This may represent either a |base::ReadOnlySharedMemoryMapping| OR a
// |base::WritableSharedMemoryMapping|, and it supports non-page-aligned base
// offsets for convenience.
class MOJO_SYSTEM_IMPL_EXPORT PlatformSharedMemoryMapping {
 public:
  PlatformSharedMemoryMapping(base::subtle::PlatformSharedMemoryRegion* region,
                              size_t offset,
                              size_t length);

  PlatformSharedMemoryMapping(const PlatformSharedMemoryMapping&) = delete;
  PlatformSharedMemoryMapping& operator=(const PlatformSharedMemoryMapping&) =
      delete;

  ~PlatformSharedMemoryMapping();

  bool IsValid() const;

  void* GetBase() const;
  size_t GetLength() const;

 private:
  absl::variant<absl::monostate,
                base::ReadOnlySharedMemoryMapping,
                base::WritableSharedMemoryMapping>
      mapping_;
};

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_PLATFORM_SHARED_MEMORY_MAPPING_H_
