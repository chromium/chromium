// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_PROCESS_PRIORITY_TRACKER_H_
#define CONTENT_COMMON_PROCESS_PRIORITY_TRACKER_H_

#include <optional>

#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/observer_list_threadsafe.h"
#include "base/process/process.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "content/common/content_export.h"

namespace content {

// This class represents overall on-screen priority for a given process of the
// embedding application. Currently only works/is used in the browser process on
// Android (Chrome and WebView).
// TODO(crbug.com/40168826): implement usage in other processes.
// Observers can be added from any sequence, but OnProcessPriorityChanged()
// should be called from the thread that created the tracker instance.
class CONTENT_EXPORT ProcessPriorityTracker {
 public:
  // Creates the tracker on the current thread. This should be the same thread
  // that calls OnProcessPriorityChanged(), usually the main thread.
  static ProcessPriorityTracker* GetInstance();

  class ProcessPriorityObserver : public base::CheckedObserver {
   public:
    // Refer to Process::Priority for meaning of invidual enum values.
    // In general, `process_priority != Priority::kBestEffort` determines
    // whether the process is foreground or background.
    virtual void OnPriorityChanged(
        base::Process::Priority process_priority) = 0;
  };

  // Should be called when the embedder app changes priority, generally based on
  // visibility, from the thread that created the tracker. Refer to
  // Process::Priority for meaning of individual enum values. Notifies the
  // registered observers of the change.
  void OnProcessPriorityChanged(base::Process::Priority process_priority);

  // Can be called from any thread.
  void AddObserver(ProcessPriorityObserver* observer);
  void RemoveObserver(ProcessPriorityObserver* observer);

 private:
  friend class base::NoDestructor<ProcessPriorityTracker>;
  ProcessPriorityTracker();
  ~ProcessPriorityTracker();

  base::Lock lock_;
  std::optional<base::Process::Priority> process_priority_ GUARDED_BY(lock_);
  scoped_refptr<base::ObserverListThreadSafe<ProcessPriorityObserver>>
      observers_ GUARDED_BY(lock_);

  SEQUENCE_CHECKER(main_thread_);
};

}  // namespace content

#endif  // CONTENT_COMMON_PROCESS_PRIORITY_TRACKER_H_
