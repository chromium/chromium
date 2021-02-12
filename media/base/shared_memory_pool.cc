// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/shared_memory_pool.h"

#include "base/logging.h"

namespace {
constexpr size_t kMaxStoredBuffers = 32;
}  // namespace

namespace media {

SharedMemoryPool::SharedMemoryPool() = default;

SharedMemoryPool::~SharedMemoryPool() = default;

SharedMemoryPool::SharedMemoryHandle::SharedMemoryHandle(
    base::UnsafeSharedMemoryRegion region,
    base::WritableSharedMemoryMapping mapping,
    scoped_refptr<SharedMemoryPool> pool)
    : region_(std::move(region)),
      mapping_(std::move(mapping)),
      pool_(std::move(pool)) {
  CHECK(pool_);
  DCHECK(region_.IsValid());
  DCHECK(mapping_.IsValid());
}

SharedMemoryPool::SharedMemoryHandle::~SharedMemoryHandle() {
  pool_->ReleaseBuffer(std::move(region_), std::move(mapping_));
}

base::UnsafeSharedMemoryRegion*
SharedMemoryPool::SharedMemoryHandle::GetRegion() {
  return &region_;
}

base::WritableSharedMemoryMapping*
SharedMemoryPool::SharedMemoryHandle::GetMapping() {
  return &mapping_;
}

std::unique_ptr<SharedMemoryPool::SharedMemoryHandle>
SharedMemoryPool::MaybeAllocateBuffer(size_t region_size) {
  base::AutoLock lock(lock_);

  DCHECK_GE(region_size, 0u);
  if (is_shutdown_)
    return nullptr;

  // Only change the configured size if bigger region is requested to avoid
  // unncecessary reallocations.
  if (region_size > region_size_) {
    mappings_.clear();
    regions_.clear();
    region_size_ = region_size;
  }
  if (!regions_.empty()) {
    DCHECK_EQ(mappings_.size(), regions_.size());
    DCHECK_GE(regions_.back().GetSize(), region_size_);
    auto handle = std::make_unique<SharedMemoryHandle>(
        std::move(regions_.back()), std::move(mappings_.back()), this);
    regions_.pop_back();
    mappings_.pop_back();
    return handle;
  }

  auto region = base::UnsafeSharedMemoryRegion::Create(region_size_);
  if (!region.IsValid())
    return nullptr;

  base::WritableSharedMemoryMapping mapping = region.Map();
  if (!mapping.IsValid())
    return nullptr;

  return std::make_unique<SharedMemoryHandle>(std::move(region),
                                              std::move(mapping), this);
}

void SharedMemoryPool::Shutdown() {
  base::AutoLock lock(lock_);
  DCHECK(!is_shutdown_);
  is_shutdown_ = true;
  mappings_.clear();
  regions_.clear();
}

void SharedMemoryPool::ReleaseBuffer(
    base::UnsafeSharedMemoryRegion region,
    base::WritableSharedMemoryMapping mapping) {
  base::AutoLock lock(lock_);
  // Only return regions which are at least as big as the current configuration.
  if (is_shutdown_ || regions_.size() >= kMaxStoredBuffers ||
      !region.IsValid() || region.GetSize() < region_size_) {
    DLOG(WARNING) << "Not returning SharedMemoryRegion to the pool:"
                  << " is_shutdown: " << (is_shutdown_ ? "true" : "false")
                  << " stored regions: " << regions_.size()
                  << " configured size: " << region_size_
                  << " this region size: " << region.GetSize()
                  << " valid: " << (region.IsValid() ? "true" : "false");
    return;
  }
  regions_.emplace_back(std::move(region));
  mappings_.emplace_back(std::move(mapping));
}

}  // namespace media
