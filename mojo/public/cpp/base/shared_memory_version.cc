// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "mojo/public/cpp/base/shared_memory_version.h"

#include <atomic>
#include <limits>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "shared_memory_version.h"

namespace mojo {

namespace {

template <typename MemoryMapping>
VersionType GetSharedVersionHelper(const MemoryMapping& mapping) {
  CHECK(mapping.IsValid());

  // Relaxed memory order since only the version is stored within the region
  // and as such is the only data shared between processes. There is no
  // re-ordering to worry about.
  return mapping.template GetMemoryAs<SharedVersionType>()->load(
      std::memory_order_relaxed);
}

}  // namespace

SharedMemoryVersionController::SharedMemoryVersionController() {
  // Create a shared memory region and immediately populate it.
  mapped_region_ =
      base::ReadOnlySharedMemoryRegion::Create(sizeof(SharedVersionType));
  CHECK(mapped_region_.IsValid());
  new (mapped_region_.mapping.memory()) SharedVersionType;

  // Clients may use `kInvalidVersion` as special value to indicate the version
  // in the absence of shared memory communication. Make sure the version starts
  // at `kInitialVersion` to avoid any confusion. Relaxed memory order because
  // no other memory operation depends on the version
  mapped_region_.mapping.GetMemoryAs<SharedVersionType>()->store(
      shared_memory_version::kInitialVersion, std::memory_order_relaxed);
}

base::ReadOnlySharedMemoryRegion
SharedMemoryVersionController::GetSharedMemoryRegion() {
  CHECK(mapped_region_.IsValid());
  return mapped_region_.region.Duplicate();
}

VersionType SharedMemoryVersionController::GetSharedVersion() {
  return GetSharedVersionHelper(mapped_region_.region.Map());
}

void SharedMemoryVersionController::Increment() {
  CHECK(mapped_region_.IsValid());

  // Relaxed memory order because no other memory operation depends on the
  // version.
  const VersionType version =
      mapped_region_.mapping.GetMemoryAs<SharedVersionType>()->fetch_add(
          1, std::memory_order_relaxed);

  // The version wrapping around is not supported and should not happen.
  CHECK_LE(version, std::numeric_limits<VersionType>::max());
}

void SharedMemoryVersionController::SetVersion(VersionType version) {
  CHECK(mapped_region_.IsValid());

  // The version wrapping around is not supported and should not happen.
  CHECK_LT(version, std::numeric_limits<VersionType>::max());

  // Version cannot decrease.
  CHECK_GE(version, GetSharedVersion());

  // Relaxed memory order because no other memory operation depends on the
  // version.
  mapped_region_.mapping.GetMemoryAs<SharedVersionType>()->store(
      version, std::memory_order_relaxed);
}

SharedMemoryVersionClient::SharedMemoryVersionClient(
    base::ReadOnlySharedMemoryRegion shared_region) {
  shared_region_ = std::move(shared_region);
  read_only_mapping_ = shared_region_.Map();
}

bool SharedMemoryVersionClient::SharedVersionIsLessThan(VersionType version) {
  // Invalid version numbers cannot be compared. Default to IPC.
  if (version == shared_memory_version::kInvalidVersion) {
    return true;
  }

  return GetSharedVersion() < version;
}

bool SharedMemoryVersionClient::SharedVersionIsGreaterThan(
    VersionType version) {
  // Invalid version numbers cannot be compared. Default to IPC.
  if (version == shared_memory_version::kInvalidVersion) {
    return true;
  }

  return GetSharedVersion() > version;
}

VersionType SharedMemoryVersionClient::GetSharedVersion() {
  CHECK(read_only_mapping_.IsValid());
  return GetSharedVersionHelper(read_only_mapping_);
}

}  // namespace mojo
