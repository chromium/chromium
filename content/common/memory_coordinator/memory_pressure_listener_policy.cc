// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/memory_coordinator/memory_pressure_listener_policy.h"

#include <string_view>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/memory_pressure_listener.h"
#include "content/common/memory_coordinator/memory_coordinator_policy_manager.h"
#include "content/public/common/child_process_id.h"
#include "third_party/blink/public/common/features.h"

namespace content {

MemoryPressureListenerPolicy::MemoryPressureListenerPolicy(
    MemoryCoordinatorPolicyManager& manager)
    : PredicateMemoryCoordinatorPolicy(
          manager,
          base::BindRepeating([](uint32_t consumer_id,
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
                    blink::features::
                        kReleaseResourceStrongReferencesOnMemoryPressure)) {
              return false;
            }
            return true;
          })),
      registration_(
          base::MemoryPressureListenerTag::kMemoryPressureListenerPolicy,
          this) {}

MemoryPressureListenerPolicy::~MemoryPressureListenerPolicy() = default;

void MemoryPressureListenerPolicy::OnMemoryPressure(
    base::MemoryPressureLevel level) {
  int limit = GetMemoryLimit();

  // Always request to release memory here. The signal was originally designed
  // for the MemoryPressureListener, which never made a distinction between
  // capping memory usage and actively freeing it.
  bool release_memory = true;

  SetLimit(limit, release_memory);
}

}  // namespace content
