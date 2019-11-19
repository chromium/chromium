// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/memory_purge_manager.h"

#include "base/feature_list.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/metrics/field_trial_params.h"
#include "base/rand_util.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/page/launching_process_state.h"
#include "third_party/blink/public/platform/platform.h"

namespace blink {

namespace {

base::TimeDelta FreezePurgeMemoryAllPagesFrozenDelay() {
  static const base::FeatureParam<int>
      kFreezePurgeMemoryAllPagesFrozenDelayInMinutes{
          &blink::features::kFreezePurgeMemoryAllPagesFrozen,
          "delay-in-minutes",
          MemoryPurgeManager::kDefaultTimeToPurgeAfterFreezing};
  return base::TimeDelta::FromMinutes(
      kFreezePurgeMemoryAllPagesFrozenDelayInMinutes.Get());
}

int MinTimeToPurgeAfterBackgroundedInSeconds() {
  static const base::FeatureParam<int>
      kMinTimeToPurgeAfterBackgroundedInMinutes{
          &blink::features::kPurgeRendererMemoryWhenBackgrounded,
          "min-delay-in-minutes",
          MemoryPurgeManager::kDefaultMinTimeToPurgeAfterBackgrounded};
  return kMinTimeToPurgeAfterBackgroundedInMinutes.Get() * 60;
}

int MaxTimeToPurgeAfterBackgroundedInSeconds() {
  static const base::FeatureParam<int>
      kMaxTimeToPurgeAfterBackgroundedInMinutes{
          &blink::features::kPurgeRendererMemoryWhenBackgrounded,
          "max-delay-in-minutes",
          MemoryPurgeManager::kDefaultMaxTimeToPurgeAfterBackgrounded};
  return kMaxTimeToPurgeAfterBackgroundedInMinutes.Get() * 60;
}

}  // namespace

MemoryPurgeManager::MemoryPurgeManager(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : renderer_backgrounded_(kLaunchingProcessIsBackgrounded),
      backgrounded_purge_pending_(false),
      total_page_count_(0),
      frozen_page_count_(0) {
  purge_timer_.SetTaskRunner(task_runner);
}

MemoryPurgeManager::~MemoryPurgeManager() = default;

void MemoryPurgeManager::OnPageCreated(PageLifecycleState state) {
  total_page_count_++;
  if (state == PageLifecycleState::kFrozen) {
    frozen_page_count_++;
  } else {
    base::MemoryPressureListener::SetNotificationsSuppressed(false);
  }

  if (!CanPurge())
    purge_timer_.Stop();
}

void MemoryPurgeManager::OnPageDestroyed(PageLifecycleState state) {
  DCHECK_GT(total_page_count_, 0);
  DCHECK_GE(frozen_page_count_, 0);
  total_page_count_--;
  if (state == PageLifecycleState::kFrozen)
    frozen_page_count_--;
  DCHECK_LE(frozen_page_count_, total_page_count_);
}

void MemoryPurgeManager::OnPageFrozen() {
  DCHECK_LT(frozen_page_count_, total_page_count_);
  frozen_page_count_++;

  if (CanPurge())
    RequestMemoryPurgeWithDelay(FreezePurgeMemoryAllPagesFrozenDelay());
}

void MemoryPurgeManager::OnPageResumed() {
  DCHECK_GT(frozen_page_count_, 0);
  frozen_page_count_--;

  if (!CanPurge())
    purge_timer_.Stop();

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
  if (!base::FeatureList::IsEnabled(
          features::kPurgeRendererMemoryWhenBackgrounded))
    return;

  backgrounded_purge_pending_ = true;
  RequestMemoryPurgeWithDelay(GetTimeToPurgeAfterBackgrounded());
}

void MemoryPurgeManager::OnRendererForegrounded() {
  backgrounded_purge_pending_ = false;
  purge_timer_.Stop();
}

void MemoryPurgeManager::RequestMemoryPurgeWithDelay(base::TimeDelta delay) {
  if (purge_timer_.IsRunning() &&
      (purge_timer_.desired_run_time() - base::TimeTicks::Now()) < delay)
    return;

  purge_timer_.Start(FROM_HERE, delay, this,
                     &MemoryPurgeManager::PerformMemoryPurge);
}

void MemoryPurgeManager::PerformMemoryPurge() {
  DCHECK(CanPurge());

  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);

  if (AreAllPagesFrozen())
    base::MemoryPressureListener::SetNotificationsSuppressed(true);

  if (backgrounded_purge_pending_) {
    Platform::Current()->RecordMetricsForBackgroundedRendererPurge();
    backgrounded_purge_pending_ = false;
  }
}

bool MemoryPurgeManager::CanPurge() const {
  if (backgrounded_purge_pending_)
    return true;

  if (!renderer_backgrounded_)
    return false;

  if (!AreAllPagesFrozen() && base::FeatureList::IsEnabled(
                                  features::kFreezePurgeMemoryAllPagesFrozen)) {
    return false;
  }

  return true;
}

bool MemoryPurgeManager::AreAllPagesFrozen() const {
  return total_page_count_ == frozen_page_count_;
}

base::TimeDelta MemoryPurgeManager::GetTimeToPurgeAfterBackgrounded() const {
  int min_time_in_seconds = MinTimeToPurgeAfterBackgroundedInSeconds();
  int max_time_in_seconds = MaxTimeToPurgeAfterBackgroundedInSeconds();
  return base::TimeDelta::FromSeconds(
      base::RandInt(min_time_in_seconds, max_time_in_seconds));
}

}  // namespace blink
