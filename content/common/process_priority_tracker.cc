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

ProcessPriorityTracker::ProcessPriorityTracker() = default;

ProcessPriorityTracker::~ProcessPriorityTracker() = default;

void ProcessPriorityTracker::AddObserver(ProcessPriorityObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_);
  observers_.AddObserver(observer);
  if (process_priority_.has_value()) {
    observer->OnPriorityChanged(*process_priority_);
  }
}

void ProcessPriorityTracker::RemoveObserver(ProcessPriorityObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_);
  observers_.RemoveObserver(observer);
}

void ProcessPriorityTracker::OnProcessPriorityChanged(
    base::Process::Priority process_priority) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_);
  if (process_priority_ == process_priority) {
    return;
  }

  process_priority_ = process_priority;
  observers_.Notify(&ProcessPriorityObserver::OnPriorityChanged,
                    process_priority);
}

}  // namespace content
