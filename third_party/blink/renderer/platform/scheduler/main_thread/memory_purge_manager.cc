// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/memory_purge_manager.h"

#include "base/feature_list.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/metrics/field_trial_params.h"
#include "base/rand_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "third_party/blink/public/platform/platform.h"

namespace blink {

namespace {

BASE_FEATURE(kMemoryPurgeInBackground,
             "MemoryPurgeInBackground",
             base::FEATURE_ENABLED_BY_DEFAULT);

// The time of first purging after a renderer is backgrounded. The value was
// initially set to 30 minutes, but it was reduced to 1 minute because this
// reduced the memory usage of a renderer 15 minutes after it was backgrounded.
//
// Experiment results:
// https://docs.google.com/document/d/1E88EYNlZE1DhmlgmjUnGnCAASm8-tWCAWXy8p53vmwc/edit?usp=sharing
const base::FeatureParam<base::TimeDelta> kMemoryPurgeInBackgroundMinDelay{
    &kMemoryPurgeInBackground, "memory_purge_background_min_delay",
    base::Minutes(1)};
const base::FeatureParam<base::TimeDelta> kMemoryPurgeInBackgroundMaxDelay{
    &kMemoryPurgeInBackground, "memory_purge_background_max_delay",
    MemoryPurgeManager::kDefaultMaxTimeToPurgeAfterBackgrounded};

}  // namespace

MemoryPurgeManager::MemoryPurgeManager(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  purge_timer_.SetTaskRunner(task_runner);
}

MemoryPurgeManager::~MemoryPurgeManager() = default;

void MemoryPurgeManager::OnPageCreated() {
  total_page_count_++;
  base::MemoryPressureListener::SetNotificationsSuppressed(false);

  if (!CanPurge()) {
    purge_timer_.Stop();
  }
}

void MemoryPurgeManager::OnPageDestroyed(bool frozen) {
  DCHECK_GT(total_page_count_, 0);
  DCHECK_GE(frozen_page_count_, 0);
  total_page_count_--;
  if (frozen) {
    frozen_page_count_--;
  }

  if (!CanPurge()) {
    purge_timer_.Stop();
  }

  DCHECK_LE(frozen_page_count_, total_page_count_);
}

void MemoryPurgeManager::OnPageFrozen(
    base::MemoryReductionTaskContext called_from) {
  DCHECK_LT(frozen_page_count_, total_page_count_);
  frozen_page_count_++;

  if (CanPurge()) {
    if (called_from == base::MemoryReductionTaskContext::kProactive) {
      PerformMemoryPurge();
    } else {
      RequestMemoryPurgeWithDelay(kFreezePurgeDelay);
    }
  }
}

void MemoryPurgeManager::OnPageResumed() {
  DCHECK_GT(frozen_page_count_, 0);
  frozen_page_count_--;

  if (!CanPurge()) {
    purge_timer_.Stop();
  }

  base::MemoryPressureListener::SetNotificationsSuppressed(false);
}

void MemoryPurgeManager::SetRendererBackgrounded(bool backgrounded) {
  renderer_backgrounded_ = backgrounded;
  if (backgrounded) {
    OnRendererBackgrounded();
  } else {
    OnRendererForegrounded();
  }
}

void MemoryPurgeManager::OnRendererBackgrounded() {
  if (!kPurgeEnabled || purge_disabled_for_testing_) {
    return;
  }

  // A spare renderer has no pages. We would like to avoid purging memory
  // on a spare renderer.
  if (total_page_count_ == 0) {
    return;
  }

  if (base::FeatureList::IsEnabled(kMemoryPurgeInBackground)) {
    backgrounded_purge_pending_ = true;
    RequestMemoryPurgeWithDelay(GetTimeToPurgeAfterBackgrounded());
  }
}

void MemoryPurgeManager::OnRendererForegrounded() {
  backgrounded_purge_pending_ = false;
  purge_timer_.Stop();
}

void MemoryPurgeManager::RequestMemoryPurgeWithDelay(base::TimeDelta delay) {
  if (!purge_timer_.IsRunning()) {
    purge_timer_.Start(FROM_HERE, delay, this,
                       &MemoryPurgeManager::PerformMemoryPurge);
  }
}

void MemoryPurgeManager::PerformMemoryPurge() {
  TRACE_EVENT0("blink", "MemoryPurgeManager::PerformMemoryPurge()");
  DCHECK(CanPurge());

  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);

  if (AreAllPagesFrozen()) {
    base::MemoryPressureListener::SetNotificationsSuppressed(true);
  }
  backgrounded_purge_pending_ = false;
}

bool MemoryPurgeManager::CanPurge() const {
  if (total_page_count_ == 0) {
    return false;
  }

  if (backgrounded_purge_pending_) {
    return true;
  }

  if (!renderer_backgrounded_) {
    return false;
  }

  return true;
}

bool MemoryPurgeManager::AreAllPagesFrozen() const {
  return total_page_count_ == frozen_page_count_;
}

base::TimeDelta MemoryPurgeManager::GetTimeToPurgeAfterBackgrounded() const {
  return base::Seconds(base::RandInt(
      static_cast<int>(kMemoryPurgeInBackgroundMinDelay.Get().InSeconds()),
      static_cast<int>(kMemoryPurgeInBackgroundMaxDelay.Get().InSeconds())));
}

}  // namespace blink
