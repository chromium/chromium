// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/memory_coordinator/memory_pressure_listener_policy.h"

#include <string_view>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory_coordinator/memory_limit.h"
#include "base/memory_coordinator/traits.h"
#include "base/notreached.h"
#include "content/common/memory_coordinator/memory_coordinator_policy_manager.h"
#include "content/public/common/child_process_id.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {

// Returns true if the consumer is targeted by this policy.
bool IsTargetConsumer(uint32_t consumer_id,
                      std::string_view consumer_name,
                      base::MemoryConsumerTraits traits,
                      ProcessType process_type,
                      ChildProcessId child_process_id) {
  // Only target consumers in the local process.
  if (!child_process_id.is_null()) {
    return false;
  }

  // TODO(pmonette): MemoryCache is temporarily singled out here
  // because releasing strong references on generic memory
  // pressure is disabled on Mac and Windows. Remove this string
  // check once a cleaner mechanism is available.
  if (consumer_name == "MemoryCache" &&
      !base::FeatureList::IsEnabled(
          blink::features::kReleaseResourceStrongReferencesOnMemoryPressure)) {
    return false;
  }
  return true;
}

// Returns true if the consumer releases memory by writing it to disk.
bool WritesToDiskOnRelease(const base::MemoryConsumerTraits& traits) {
  switch (traits.release_memory_data_access) {
    case base::MemoryConsumerTraits::ReleaseMemoryDataAccess::kNone:
    case base::MemoryConsumerTraits::ReleaseMemoryDataAccess::kReads:
    case base::MemoryConsumerTraits::ReleaseMemoryDataAccess::kNA:
      return false;
    case base::MemoryConsumerTraits::ReleaseMemoryDataAccess::
        kReadsAndWritesToDisk:
      return true;
  }
  NOTREACHED();
}

// Returns true if the consumer is targeted by this policy and releases memory
// by writing it to disk.
bool IsDiskWritingTargetConsumer(uint32_t consumer_id,
                                 std::string_view consumer_name,
                                 base::MemoryConsumerTraits traits,
                                 ProcessType process_type,
                                 ChildProcessId child_process_id) {
  return IsTargetConsumer(consumer_id, consumer_name, traits, process_type,
                          child_process_id) &&
         WritesToDiskOnRelease(traits);
}

}  // namespace

MemoryPressureListenerPolicy::MemoryPressureListenerPolicy(
    MemoryCoordinatorPolicyManager& manager)
    : PredicateMemoryCoordinatorPolicy(manager,
                                       base::BindRepeating(&IsTargetConsumer)),
      registration_(
          base::MemoryPressureListenerTag::kMemoryPressureListenerPolicy,
          this) {}

MemoryPressureListenerPolicy::~MemoryPressureListenerPolicy() = default;

void MemoryPressureListenerPolicy::OnMemoryPressure(
    base::MemoryPressureLevel level) {
  base::MemoryLimit limit = GetMemoryLimit();

  // Request to release memory whenever under pressure. The signal was
  // originally designed for the MemoryPressureListener, which never made a
  // distinction between capping memory usage and actively freeing it.
  bool release_memory = level != base::MEMORY_PRESSURE_LEVEL_NONE;

  // Under critical memory pressure, the system is probably already swapping
  // out memory and making heavy use of disk IO. Consumers that release memory
  // by writing it to disk still get the lowered limit to stop their growth, but
  // are not asked to release memory. Doing so would force the OS to page
  // swapped-out memory back in, only to write it to disk again, compounding the
  // problem.
  bool suppress_disk_writing_releases =
      limit < base::MemoryLimit::ModeratePressureThreshold();

  // Suppress before lowering the limit so that the release request is dropped.
  if (suppress_disk_writing_releases) {
    manager().SetMemoryReleaseSuppression(
        this, base::BindRepeating(&IsDiskWritingTargetConsumer));
  }

  SetLimit(limit, release_memory);

  // Stop suppressing after updating the limit, so that the consumers are asked
  // to release memory only once and converge to their aggregate limit.
  if (!suppress_disk_writing_releases) {
    manager().ClearMemoryReleaseSuppression(this);
  }
}

}  // namespace content
