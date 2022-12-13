// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/time/time_override.h"
#include "third_party/abseil-cpp/absl/memory/memory.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"

#include "third_party/blink/renderer/platform/scheduler/common/scoped_time_source_override.h"

namespace blink::scheduler {

namespace {

using TimeSource = ScopedTimeSourceOverride::TimeSource;

class TimeSourceOverrideManager {
 public:
  static TimeSourceOverrideManager& Instance() {
    static base::NoDestructor<TimeSourceOverrideManager> instance;
    return *instance;
  }

  static WTF::ThreadSpecific<TimeSource*>& PerThreadTimeSource() {
    static base::NoDestructor<WTF::ThreadSpecific<TimeSource*>> instance;
    return *instance;
  }

  ~TimeSourceOverrideManager() = delete;

  void AddUsage();
  void ReleaseUsage();
  void SetDefaultTimeSource(TimeSource* time_source) {
    DCHECK(!default_time_source_ || !time_source)
        << "Default time source can be set only once";
    default_time_source_ = time_source;
  }

 private:
  friend class base::NoDestructor<TimeSourceOverrideManager>;
  TimeSourceOverrideManager() = default;

  static base::TimeTicks GetVirtualTimeTicks() {
    if (auto* time_source = GetTimeSourceForCurrentThread())
      return time_source->NowTicks();
    return base::subtle::TimeTicksNowIgnoringOverride();
  }

  static base::Time GetVirtualTime() {
    if (auto* time_source = GetTimeSourceForCurrentThread())
      return time_source->Date();
    return base::subtle::TimeNowIgnoringOverride();
  }

  static TimeSource* GetTimeSourceForCurrentThread() {
    auto& per_thread = PerThreadTimeSource();
    if (per_thread.IsSet() && *per_thread)
      return *per_thread;
    return Instance().default_time_source_;
  }

  std::atomic<TimeSource*> default_time_source_ = nullptr;
  base::Lock lock_;
  int active_overrides_ GUARDED_BY(lock_) = 0;
  std::unique_ptr<base::subtle::ScopedTimeClockOverrides> time_overrides_
      GUARDED_BY(lock_);
};

void TimeSourceOverrideManager::AddUsage() {
  base::AutoLock auto_lock(lock_);
  if (active_overrides_++)
    return;
  DCHECK(!time_overrides_);
  time_overrides_ = std::make_unique<base::subtle::ScopedTimeClockOverrides>(
      TimeSourceOverrideManager::GetVirtualTime,
      TimeSourceOverrideManager::GetVirtualTimeTicks, nullptr);
}

void TimeSourceOverrideManager::ReleaseUsage() {
  base::AutoLock auto_lock(lock_);
  if (--active_overrides_)
    return;
  DCHECK(time_overrides_);
  time_overrides_.reset();
}

}  // namespace

// static
std::unique_ptr<ScopedTimeSourceOverride>
ScopedTimeSourceOverride::CreateDefault(TimeSource& time_source) {
  TimeSourceOverrideManager::Instance().SetDefaultTimeSource(&time_source);
  return base::WrapUnique(new ScopedTimeSourceOverride(/* is_default */ true));
}

// static
std::unique_ptr<ScopedTimeSourceOverride>
ScopedTimeSourceOverride::CreateForCurrentThread(TimeSource& time_source) {
  auto& per_thread = TimeSourceOverrideManager::PerThreadTimeSource();
  DCHECK(!per_thread.IsSet() || !*per_thread);
  *per_thread = &time_source;
  return base::WrapUnique(new ScopedTimeSourceOverride(/* is_default */ false));
}

ScopedTimeSourceOverride::ScopedTimeSourceOverride(bool is_default)
    : is_default_(is_default) {
  TimeSourceOverrideManager::Instance().AddUsage();
}

ScopedTimeSourceOverride::~ScopedTimeSourceOverride() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  TimeSourceOverrideManager::Instance().ReleaseUsage();

  if (is_default_) {
    TimeSourceOverrideManager::Instance().SetDefaultTimeSource(nullptr);
  } else {
    auto& per_thread_time_source =
        TimeSourceOverrideManager::PerThreadTimeSource();
    DCHECK(per_thread_time_source.IsSet());
    *per_thread_time_source = nullptr;
  }
}

}  // namespace blink::scheduler
