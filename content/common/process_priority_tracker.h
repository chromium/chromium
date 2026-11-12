// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_PROCESS_PRIORITY_TRACKER_H_
#define CONTENT_COMMON_PROCESS_PRIORITY_TRACKER_H_

#include <optional>

#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/process/process.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "content/common/content_export.h"

namespace content {

// This class represents overall on-screen priority for a given process of the
// embedding application. Currently only works/is used in the browser process on
// Android (Chrome and WebView).
// TODO(crbug.com/40168826): implement usage in other processes.
// This class must only be used on the main thread. Observers are notified
// synchronously on that thread; observers that live on another sequence must
// post to it.
class CONTENT_EXPORT ProcessPriorityTracker {
 public:
  // Returns the process-wide instance. Must only be called on the main thread.
  static ProcessPriorityTracker* GetInstance();

  class ProcessPriorityObserver : public base::CheckedObserver {
   public:
    // Refer to Process::Priority for meaning of individual enum values.
    // In general, `process_priority != Priority::kBestEffort` determines
    // whether the process is foreground or background.
    virtual void OnPriorityChanged(
        base::Process::Priority process_priority) = 0;
  };

  // Should be called when the embedder app changes priority, generally based on
  // visibility. Refer to Process::Priority for meaning of individual enum
  // values. Synchronously notifies the registered observers of the change.
  void OnProcessPriorityChanged(base::Process::Priority process_priority);

  // Adds/removes an observer. If the priority is already known, `observer` is
  // synchronously notified of it when added.
  void AddObserver(ProcessPriorityObserver* observer);
  void RemoveObserver(ProcessPriorityObserver* observer);

 private:
  friend class base::NoDestructor<ProcessPriorityTracker>;
  ProcessPriorityTracker();
  ~ProcessPriorityTracker();

  SEQUENCE_CHECKER(main_thread_);

  std::optional<base::Process::Priority> process_priority_
      GUARDED_BY_CONTEXT(main_thread_);
  base::ObserverList<ProcessPriorityObserver> observers_
      GUARDED_BY_CONTEXT(main_thread_);
};

}  // namespace content

#endif  // CONTENT_COMMON_PROCESS_PRIORITY_TRACKER_H_
