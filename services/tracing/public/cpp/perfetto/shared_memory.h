// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_SHARED_MEMORY_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_SHARED_MEMORY_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/shared_memory.h"

namespace tracing {

// This wraps //base's shmem implementation for Perfetto to consume.
class COMPONENT_EXPORT(TRACING_CPP) ChromeBaseSharedMemory
    : public perfetto::SharedMemory {
 public:
  class COMPONENT_EXPORT(TRACING_CPP) Factory
      : public perfetto::SharedMemory::Factory {
   public:
    std::unique_ptr<perfetto::SharedMemory> CreateSharedMemory(
        size_t size) override;
  };

  explicit ChromeBaseSharedMemory(size_t size);
  explicit ChromeBaseSharedMemory(base::UnsafeSharedMemoryRegion region);

  ChromeBaseSharedMemory(const ChromeBaseSharedMemory&) = delete;
  ChromeBaseSharedMemory& operator=(const ChromeBaseSharedMemory&) = delete;

  ~ChromeBaseSharedMemory() override;

  // Clone the region, e.g. for sending to other processes over IPC.
  base::UnsafeSharedMemoryRegion CloneRegion();

  const base::UnsafeSharedMemoryRegion& region() const { return region_; }

  // perfetto::SharedMemory implementation. Called internally by Perfetto
  // classes.
  void* start() const override;
  size_t size() const override;

 private:
  base::UnsafeSharedMemoryRegion region_;
  base::WritableSharedMemoryMapping mapping_;
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_SHARED_MEMORY_H_
