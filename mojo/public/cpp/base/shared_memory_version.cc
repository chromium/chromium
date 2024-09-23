// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "mojo/public/cpp/base/shared_memory_version.h"

#include <atomic>
#include <limits>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/structured_shared_memory.h"

namespace mojo {

SharedMemoryVersionController::SharedMemoryVersionController()
    : mapped_region_(
          // Clients may use `kInvalidVersion` as a special value to indicate
          // the version in the absence of shared memory communication. Make
          // sure the version starts at `kInitialVersion` to avoid any
          // confusion.
          base::AtomicSharedMemory<VersionType>::Create(
              shared_memory_version::kInitialVersion)
              .value()) {}

SharedMemoryVersionController::~SharedMemoryVersionController() = default;

base::ReadOnlySharedMemoryRegion
SharedMemoryVersionController::GetSharedMemoryRegion() const {
  return mapped_region_.DuplicateReadOnlyRegion();
}

VersionType SharedMemoryVersionController::GetSharedVersion() const {
  // Relaxed memory order because no other memory operation depends on the
  // version.
  return mapped_region_.WritableRef().load(std::memory_order_relaxed);
}

void SharedMemoryVersionController::Increment() {
  // Relaxed memory order because no other memory operation depends on the
  // version.
  const VersionType version =
      mapped_region_.WritableRef().fetch_add(1, std::memory_order_relaxed);

  // The version wrapping around is not supported and should not happen.
  CHECK_LE(version, std::numeric_limits<VersionType>::max());
}

void SharedMemoryVersionController::SetVersion(VersionType version) {
  // The version wrapping around is not supported and should not happen.
  CHECK_LT(version, std::numeric_limits<VersionType>::max());

  // Version cannot decrease.
  CHECK_GE(version, GetSharedVersion());

  // Relaxed memory order because no other memory operation depends on the
  // version.
  mapped_region_.WritableRef().store(version, std::memory_order_relaxed);
}

SharedMemoryVersionClient::SharedMemoryVersionClient(
    base::ReadOnlySharedMemoryRegion shared_region)
    : read_only_mapping_(
          base::AtomicSharedMemory<VersionType>::MapReadOnlyRegion(
              std::move(shared_region))
              .value()) {}

SharedMemoryVersionClient::~SharedMemoryVersionClient() = default;

bool SharedMemoryVersionClient::SharedVersionIsLessThan(
    VersionType version) const {
  // Invalid version numbers cannot be compared. Default to IPC.
  if (version == shared_memory_version::kInvalidVersion) {
    return true;
  }

  return GetSharedVersion() < version;
}

bool SharedMemoryVersionClient::SharedVersionIsGreaterThan(
    VersionType version) const {
  // Invalid version numbers cannot be compared. Default to IPC.
  if (version == shared_memory_version::kInvalidVersion) {
    return true;
  }

  return GetSharedVersion() > version;
}

VersionType SharedMemoryVersionClient::GetSharedVersion() const {
  // Relaxed memory order because no other memory operation depends on the
  // version.
  return read_only_mapping_.ReadOnlyRef().load(std::memory_order_relaxed);
}

}  // namespace mojo
