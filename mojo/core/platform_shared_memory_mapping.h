// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_PLATFORM_SHARED_MEMORY_MAPPING_H_
#define MOJO_CORE_PLATFORM_SHARED_MEMORY_MAPPING_H_

#include <stddef.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "mojo/core/system_impl_export.h"

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
  enum class Type {
    kReadOnly,
    kWritable,
  };

  PlatformSharedMemoryMapping(base::subtle::PlatformSharedMemoryRegion* region,
                              size_t offset,
                              size_t length);
  ~PlatformSharedMemoryMapping();

  bool IsValid() const;

  void* GetBase() const;
  size_t GetLength() const;

 private:
  const Type type_;
  const size_t offset_;
  const size_t length_;
  void* base_ = nullptr;
  std::unique_ptr<base::SharedMemoryMapping> mapping_;

  DISALLOW_COPY_AND_ASSIGN(PlatformSharedMemoryMapping);
};

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_PLATFORM_SHARED_MEMORY_MAPPING_H_
