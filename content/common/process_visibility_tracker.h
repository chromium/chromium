// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_PROCESS_VISIBILITY_TRACKER_H_
#define CONTENT_COMMON_PROCESS_VISIBILITY_TRACKER_H_

#include <optional>

#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/observer_list_threadsafe.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "content/common/content_export.h"

namespace content {

// This class represents overall on-screen visibility for a given process of the
// embedding application. Currently only works/is used in the browser process on
// Android (Chrome and WebView).
// TODO(crbug.com/40168826): implement usage in other processes.
// Observers can be added from any sequence, but OnProcessVisibilityChanged()
// should be called from the thread that created the tracker instance.
class CONTENT_EXPORT ProcessVisibilityTracker {
 public:
  // Creates the tracker on the current thread. This should be the same thread
  // that calls OnProcessVisibilityChanged(), usually the main thread.
  static ProcessVisibilityTracker* GetInstance();

  class ProcessVisibilityObserver : public base::CheckedObserver {
   public:
    virtual void OnVisibilityChanged(bool visible) = 0;
  };

  // Should be called when the embedder app becomes visible or invisible, from
  // the thread that created the tracker. Notifies the registered observers of
  // the change.
  void OnProcessVisibilityChanged(bool visible);

  // Can be called from any thread.
  void AddObserver(ProcessVisibilityObserver* observer);
  void RemoveObserver(ProcessVisibilityObserver* observer);

 private:
  friend class base::NoDestructor<ProcessVisibilityTracker>;
  ProcessVisibilityTracker();
  ~ProcessVisibilityTracker();

  base::Lock lock_;
  std::optional<bool> is_visible_ GUARDED_BY(lock_);
  scoped_refptr<base::ObserverListThreadSafe<ProcessVisibilityObserver>>
      observers_ GUARDED_BY(lock_);

  SEQUENCE_CHECKER(main_thread_);
};

}  // namespace content

#endif  // CONTENT_COMMON_PROCESS_VISIBILITY_TRACKER_H_
