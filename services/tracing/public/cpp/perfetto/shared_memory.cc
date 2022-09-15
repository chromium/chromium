// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/shared_memory.h"

#include <utility>

#include "third_party/perfetto/include/perfetto/ext/tracing/core/shared_memory.h"

namespace tracing {

std::unique_ptr<perfetto::SharedMemory>
ChromeBaseSharedMemory::Factory::CreateSharedMemory(size_t size) {
  return std::make_unique<ChromeBaseSharedMemory>(size);
}

ChromeBaseSharedMemory::ChromeBaseSharedMemory(size_t size)
    : region_(base::UnsafeSharedMemoryRegion::Create(size)) {
  // DCHECK rather than CHECK as we handle SMB creation failures as
  // DumpWithoutCrashing in ProducerClient on release builds.
  DCHECK(region_.IsValid());
  mapping_ = region_.Map();
  DCHECK(mapping_.IsValid());
}

ChromeBaseSharedMemory::ChromeBaseSharedMemory(
    base::UnsafeSharedMemoryRegion region)
    : region_(std::move(region)) {
  mapping_ = region_.Map();
  // DCHECK rather than CHECK as we handle SMB mapping failures in ProducerHost
  // on release builds.
  DCHECK(mapping_.IsValid());
}

ChromeBaseSharedMemory::~ChromeBaseSharedMemory() = default;

base::UnsafeSharedMemoryRegion ChromeBaseSharedMemory::CloneRegion() {
  return region_.Duplicate();
}

void* ChromeBaseSharedMemory::start() const {
  return mapping_.memory();
}

size_t ChromeBaseSharedMemory::size() const {
  return region_.GetSize();
}

}  // namespace tracing
