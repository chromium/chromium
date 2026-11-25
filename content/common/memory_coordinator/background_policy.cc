// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/memory_coordinator/background_policy.h"

#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/notreached.h"
#include "content/common/memory_coordinator/memory_coordinator_policy_manager.h"
#include "content/public/common/child_process_id.h"
#include "content/public/common/memory_consumer_update.h"
#include "content/public/common/process_type.h"

namespace content {

namespace {

using AffectedByBackgrounding =
    base::MemoryConsumerTraits::AffectedByBackgrounding;

base::TimeDelta GetReleaseDelay(
    AffectedByBackgrounding affected_by_backgrounding) {
  switch (affected_by_backgrounding) {
    case AffectedByBackgrounding::kAfterShortDelay:
      return BackgroundPolicy::kShortDelay;
    case AffectedByBackgrounding::kAfterMediumDelay:
      return BackgroundPolicy::kMediumDelay;
    case AffectedByBackgrounding::kAfterLongDelay:
      return BackgroundPolicy::kLongDelay;
    case AffectedByBackgrounding::kNo:
      break;
  }
  NOTREACHED();
}

// Returns true if the consumer is targeted by this policy. Only consumers in
// the local process are targeted, since child processes have their own
// BackgroundPolicy.
bool IsTargetConsumer(base::MemoryConsumerTraits traits,
                      ChildProcessId child_process_id) {
  return child_process_id.is_null() &&
         traits.affected_by_backgrounding != AffectedByBackgrounding::kNo;
}

}  // namespace

BackgroundPolicy::BackgroundPolicy(MemoryCoordinatorPolicyManager& manager)
    : MemoryCoordinatorPolicy(manager), registration_(manager, *this) {}

BackgroundPolicy::~BackgroundPolicy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void BackgroundPolicy::OnProcessPriorityChanged(
    base::Process::Priority process_priority) {
  if (process_priority == base::Process::Priority::kBestEffort) {
    OnBackgrounded();
  } else {
    OnForegrounded();
  }
}

void BackgroundPolicy::OnBackgrounded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_backgrounded_) {
    return;
  }
  is_backgrounded_ = true;

  // Lowering the limit only stops consumers from growing. Memory is released
  // once their delay expires.
  manager().UpdateConsumers(
      this,
      [](uint32_t consumer_id, std::string_view consumer_name,
         base::MemoryConsumerTraits traits, ProcessType process_type,
         ChildProcessId child_process_id) {
        return IsTargetConsumer(traits, child_process_id);
      },
      kBackgroundLimit, /*release_memory=*/false);

  for (AffectedByBackgrounding affected_by_backgrounding :
       DelayedValues::All()) {
    GetReleaseTimer(affected_by_backgrounding)
        .Start(
            FROM_HERE, GetReleaseDelay(affected_by_backgrounding),
            base::BindOnce(&BackgroundPolicy::OnReleaseDelayExpired,
                           base::Unretained(this), affected_by_backgrounding));
  }
}

void BackgroundPolicy::OnForegrounded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_backgrounded_) {
    return;
  }
  is_backgrounded_ = false;

  for (base::OneShotDelayedBackgroundTimer& timer : release_timers_) {
    timer.Stop();
  }

  manager().UpdateConsumers(
      this,
      [](uint32_t consumer_id, std::string_view consumer_name,
         base::MemoryConsumerTraits traits, ProcessType process_type,
         ChildProcessId child_process_id) {
        return IsTargetConsumer(traits, child_process_id);
      },
      base::MemoryLimit::Default(), /*release_memory=*/false);
}

void BackgroundPolicy::OnConsumerGroupAdded(uint32_t consumer_id,
                                            std::string_view consumer_name,
                                            base::MemoryConsumerTraits traits,
                                            ProcessType process_type,
                                            ChildProcessId child_process_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_backgrounded_ || !IsTargetConsumer(traits, child_process_id)) {
    return;
  }

  // A new group has had little or no time to grow, so it only needs the limit,
  // even if its delay already expired. A release request sent here would be
  // dropped anyway, since the consumer is still being registered.
  std::vector<MemoryConsumerUpdate> updates;
  updates.push_back({consumer_id, kBackgroundLimit, /*release_memory=*/false});
  manager().UpdateConsumers(this, std::move(updates));
}

void BackgroundPolicy::OnConsumerGroupRemoved(uint32_t consumer_id,
                                              ChildProcessId child_process_id) {
}

base::OneShotDelayedBackgroundTimer& BackgroundPolicy::GetReleaseTimer(
    AffectedByBackgrounding affected_by_backgrounding) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(DelayedValues::All().Has(affected_by_backgrounding));
  return release_timers_[std::to_underlying(affected_by_backgrounding) -
                         std::to_underlying(DelayedValues::kMinValue)];
}

void BackgroundPolicy::OnReleaseDelayExpired(
    AffectedByBackgrounding affected_by_backgrounding) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(is_backgrounded_);
  // The limit was already lowered, so the manager only sends the release
  // request.
  manager().UpdateConsumers(
      this,
      [affected_by_backgrounding](
          uint32_t consumer_id, std::string_view consumer_name,
          base::MemoryConsumerTraits traits, ProcessType process_type,
          ChildProcessId child_process_id) {
        return IsTargetConsumer(traits, child_process_id) &&
               traits.affected_by_backgrounding == affected_by_backgrounding;
      },
      kBackgroundLimit, /*release_memory=*/true);
}

}  // namespace content
