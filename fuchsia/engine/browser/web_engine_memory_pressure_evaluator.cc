// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/web_engine_memory_pressure_evaluator.h"

#include <lib/zx/job.h>
#include <lib/zx/process.h>
#include <zircon/rights.h>

#include "base/fuchsia/default_job.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/util/memory_pressure/memory_pressure_voter.h"

namespace {

// TODO(https://crbug.com/1020698): Connect the evaluator to OS-provided metrics
// or signals, rather than hard-wiring a target Context memory usage.
constexpr size_t kContextMemoryCriticalBytes = 150 * 1024 * 1024;
constexpr size_t kContextMemoryModerateBytes =
    (kContextMemoryCriticalBytes * 3) / 4;

// Match the moderate-pressure "cooldown" used on Windows & ChromeOS.
constexpr auto kModerateCooldownTime = base::TimeDelta::FromSeconds(10);

size_t QueryJobMemoryBytes(zx::unowned_job job) {
  size_t total_bytes = 0u;

  // Enumerate size of processes under |job|.
  zx_koid_t koids[128]{};
  size_t actual = 0u;
  zx_status_t status = job->get_info(ZX_INFO_JOB_PROCESSES, koids,
                                     sizeof(koids), &actual, nullptr);
  ZX_CHECK(status == ZX_OK, status) << "get_info(PROCESSES)";
  for (size_t i = 0; i < actual; ++i) {
    zx::process process;
    if (job->get_child(koids[i], ZX_RIGHT_SAME_RIGHTS, &process) != ZX_OK)
      continue;
    zx_info_task_stats_t context_stats{};
    zx_status_t status =
        process.get_info(ZX_INFO_TASK_STATS, &context_stats,
                         sizeof(context_stats), nullptr, nullptr);
    if (status != ZX_OK)
      continue;
    total_bytes +=
        context_stats.mem_private_bytes + context_stats.mem_scaled_shared_bytes;
  }

  // Recursively enumerate jobs under |job|.
  status = job->get_info(ZX_INFO_JOB_CHILDREN, koids, sizeof(koids), &actual,
                         nullptr);
  ZX_CHECK(status == ZX_OK, status) << "get_info(CHILDREN)";
  for (size_t i = 0; i < actual; ++i) {
    zx::job child_job;
    if (job->get_child(koids[i], ZX_RIGHT_SAME_RIGHTS, &child_job) != ZX_OK)
      continue;
    total_bytes += QueryJobMemoryBytes(zx::unowned_job(child_job));
  }

  return total_bytes;
}

}  // namespace

WebEngineMemoryPressureEvaluator::WebEngineMemoryPressureEvaluator(
    std::unique_ptr<util::MemoryPressureVoter> voter)
    : util::SystemMemoryPressureEvaluator(std::move(voter)) {
  timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(1),
               base::BindRepeating(
                   &WebEngineMemoryPressureEvaluator::CheckMemoryPressure,
                   base::Unretained(this)));
  CheckMemoryPressure();
}

WebEngineMemoryPressureEvaluator::~WebEngineMemoryPressureEvaluator() = default;

void WebEngineMemoryPressureEvaluator::CheckMemoryPressure() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::MemoryPressureListener::MemoryPressureLevel old_level = current_vote();

  // Query the private and shared memory usage of processes in the Context job.
  size_t context_bytes = QueryJobMemoryBytes(base::GetDefaultJob());

  // Calculate a MemoryPressureLevel based on current Context usage.
  base::MemoryPressureListener::MemoryPressureLevel new_level =
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
  if (context_bytes >= kContextMemoryCriticalBytes) {
    new_level = base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL;
  } else if (context_bytes > kContextMemoryModerateBytes) {
    new_level = base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE;
  }

  VLOG(1) << "Context memory: " << context_bytes << ", pressure=" << new_level;

  // Set the new vote, and determine whether to notify listeners.
  SetCurrentVote(new_level);
  switch (new_level) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
      // By convention no notifications are sent when returning to NONE level.
      SendCurrentVote(false);
      break;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE: {
      // Moderate pressure signals trigger some cleanup work, so to avoid
      // impacting performance, only deliver MODERATE signals if it that level
      // is sustained.
      if (old_level != new_level)
        next_moderate_notify_time_ = base::TimeTicks();
      base::TimeTicks now = base::TimeTicks::Now();
      SendCurrentVote(now >= next_moderate_notify_time_);
      next_moderate_notify_time_ = now + kModerateCooldownTime;
      break;
    }
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      SendCurrentVote(true);
      break;
  }
}
