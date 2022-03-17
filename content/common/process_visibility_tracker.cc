// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/process_visibility_tracker.h"

#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "components/power_scheduler/power_mode_arbiter.h"

namespace content {

// static
ProcessVisibilityTracker* ProcessVisibilityTracker::GetInstance() {
  static base::NoDestructor<ProcessVisibilityTracker> instance;
  return instance.get();
}

ProcessVisibilityTracker::ProcessVisibilityTracker()
    : power_mode_visibility_voter_(
          power_scheduler::PowerModeArbiter::GetInstance()->NewVoter(
              "PowerModeVoter.Visibility")) {}

ProcessVisibilityTracker::~ProcessVisibilityTracker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_);
}

void ProcessVisibilityTracker::AddObserver(
    ProcessVisibilityObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_);

  observers_.AddObserver(observer);
  if (is_visible_.has_value())
    observer->OnVisibilityChanged(*is_visible_);
}

void ProcessVisibilityTracker::RemoveObserver(
    ProcessVisibilityObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_);

  observers_.RemoveObserver(observer);
}

void ProcessVisibilityTracker::OnProcessVisibilityChanged(bool visible) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_);
  if (is_visible_.has_value() && *is_visible_ == visible)
    return;

  is_visible_ = visible;

  power_mode_visibility_voter_->VoteFor(
      *is_visible_ ? power_scheduler::PowerMode::kIdle
                   : power_scheduler::PowerMode::kBackground);

  for (ProcessVisibilityObserver& observer : observers_)
    observer.OnVisibilityChanged(*is_visible_);
}

}  // namespace content
