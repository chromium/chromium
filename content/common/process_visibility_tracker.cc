// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/process_visibility_tracker.h"

#include "base/no_destructor.h"

namespace content {

// static
ProcessVisibilityTracker* ProcessVisibilityTracker::GetInstance() {
  static base::NoDestructor<ProcessVisibilityTracker> instance;
  return instance.get();
}

ProcessVisibilityTracker::ProcessVisibilityTracker()
    : observers_(base::MakeRefCounted<
                 base::ObserverListThreadSafe<ProcessVisibilityObserver>>()) {}

ProcessVisibilityTracker::~ProcessVisibilityTracker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_);
}

void ProcessVisibilityTracker::AddObserver(
    ProcessVisibilityObserver* observer) {
  std::optional<bool> is_visible;
  {
    // Synchronize access to |observers_| and |is_visible_| to ensure
    // consistency in notifications to observers (in case of concurrent
    // modification of the visibility state).
    base::AutoLock lock(lock_);
    observers_->AddObserver(observer);
    is_visible = is_visible_;
  }
  // Notify outside the lock to allow the observer to call back into
  // ProcessVisibilityTracker.
  if (is_visible.has_value())
    observer->OnVisibilityChanged(*is_visible);
}

void ProcessVisibilityTracker::RemoveObserver(
    ProcessVisibilityObserver* observer) {
  base::AutoLock lock(lock_);
  observers_->RemoveObserver(observer);
}

void ProcessVisibilityTracker::OnProcessVisibilityChanged(bool visible) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_);

  {
    base::AutoLock lock(lock_);
    if (is_visible_.has_value() && *is_visible_ == visible)
      return;

    is_visible_ = visible;

    observers_->Notify(
        FROM_HERE, &ProcessVisibilityObserver::OnVisibilityChanged, visible);
  }
}

}  // namespace content
