// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory_coordinator/zero_limit_policy.h"

#include <string_view>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory_coordinator/memory_limit.h"
#include "base/memory_coordinator/traits.h"
#include "content/common/memory_coordinator/memory_coordinator_policy_manager.h"
#include "content/public/common/child_process_id.h"
#include "content/public/common/process_type.h"

namespace content {

BASE_FEATURE(kMemoryCoordinatorZeroLimit, base::FEATURE_DISABLED_BY_DEFAULT);

ZeroLimitPolicy::ZeroLimitPolicy(MemoryCoordinatorPolicyManager& manager)
    : PredicateMemoryCoordinatorPolicy(
          manager,
          base::BindRepeating([](uint32_t consumer_id,
                                 std::string_view consumer_name,
                                 base::MemoryConsumerTraits traits,
                                 ProcessType process_type,
                                 ChildProcessId child_process_id) {
            return traits.is_stateful ==
                   base::MemoryConsumerTraits::IsStateful::kYes;
          })),
      policy_registration_(manager, *this) {
  SetLimit(base::MemoryLimit::FromPercent(0), /*release_memory=*/true);
}

ZeroLimitPolicy::~ZeroLimitPolicy() = default;

}  // namespace content
