// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/process_priority_tracker.h"

#include "base/no_destructor.h"

namespace content {

// static
ProcessPriorityTracker* ProcessPriorityTracker::GetInstance() {
  static base::NoDestructor<ProcessPriorityTracker> instance;
  return instance.get();
}

ProcessPriorityTracker::ProcessPriorityTracker()
    : observers_(base::MakeRefCounted<
                 base::ObserverListThreadSafe<ProcessPriorityObserver>>()) {}

ProcessPriorityTracker::~ProcessPriorityTracker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_);
}

void ProcessPriorityTracker::AddObserver(ProcessPriorityObserver* observer) {
  std::optional<base::Process::Priority> process_priority;
  {
    // Synchronize access to |observers_| and |is_visible_| to ensure
    // consistency in notifications to observers (in case of concurrent
    // modification of the priority state).
    base::AutoLock lock(lock_);
    observers_->AddObserver(observer);
    process_priority = process_priority_;
  }
  // Notify outside the lock to allow the observer to call back into
  // ProcessPriorityTracker.
  if (process_priority.has_value()) {
    observer->OnPriorityChanged(*process_priority);
  }
}

void ProcessPriorityTracker::RemoveObserver(ProcessPriorityObserver* observer) {
  base::AutoLock lock(lock_);
  observers_->RemoveObserver(observer);
}

void ProcessPriorityTracker::OnProcessPriorityChanged(
    base::Process::Priority process_priority) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_);

  {
    base::AutoLock lock(lock_);
    if (process_priority_.has_value() &&
        *process_priority_ == process_priority) {
      return;
    }

    process_priority_ = process_priority;

    observers_->Notify(FROM_HERE, &ProcessPriorityObserver::OnPriorityChanged,
                       process_priority);
  }
}

}  // namespace content
