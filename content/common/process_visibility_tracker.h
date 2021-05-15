// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_PROCESS_VISIBILITY_TRACKER_H_
#define CONTENT_COMMON_PROCESS_VISIBILITY_TRACKER_H_

#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "components/power_scheduler/power_mode_voter.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

// This class represents overall on-screen visibility for a given process of the
// embedding application. Currently only works/is used in the browser process on
// Android (Chrome and WebView).
// TODO(crbug.com/1177542): implement usage in other processes.
// All methods should be called on the same sequence where the instance of this
// class was created. Observer callbacks will also be called on the same
// sequence. Outside tests, the ProcessVisibilityTracker instance lives on the
// process's main thread.
class CONTENT_EXPORT ProcessVisibilityTracker {
 public:
  static ProcessVisibilityTracker* GetInstance();

  class ProcessVisibilityObserver : public base::CheckedObserver {
   public:
    virtual void OnVisibilityChanged(bool visible) = 0;
  };

  // Should be called when the embedder app becomes visible or
  // invisible. Notifies the registered observers of the change.
  void OnProcessVisibilityChanged(bool visible);
  void AddObserver(ProcessVisibilityObserver* observer);
  void RemoveObserver(ProcessVisibilityObserver* observer);

 private:
  friend class base::NoDestructor<ProcessVisibilityTracker>;
  ProcessVisibilityTracker();
  ~ProcessVisibilityTracker();

  absl::optional<bool> is_visible_;
  base::ObserverList<ProcessVisibilityObserver> observers_;
  std::unique_ptr<power_scheduler::PowerModeVoter> power_mode_visibility_voter_;
  SEQUENCE_CHECKER(main_thread_);
};

}  // namespace content

#endif  // CONTENT_COMMON_PROCESS_VISIBILITY_TRACKER_H_
