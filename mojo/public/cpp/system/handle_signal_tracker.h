// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_SYSTEM_HANDLE_SIGNAL_TRACKER_H_
#define MOJO_PUBLIC_CPP_SYSTEM_HANDLE_SIGNAL_TRACKER_H_

#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "mojo/public/cpp/system/system_export.h"

namespace base {
class SequencedTaskRunner;
}

namespace mojo {

// This class helps track the state of specific signal on a handle so that
// the user doesn't have to manually query the signal state every time they
// want to know the handle's state.
//
// Usage of this class is specifically targeting cases where the signal state
// changes infrequently but must be queried frequently. If either condition does
// not hold, consider using Handle::QuerySignalsState (or
// MojoQueryHandleSignalsState) directly instead.
class MOJO_CPP_SYSTEM_EXPORT HandleSignalTracker {
 public:
  using NotificationCallback =
      base::RepeatingCallback<void(const HandleSignalsState& signals_state)>;

  // Constructs a tracker which tracks |signals| on |handle|. |signals| may
  // be any single signal flag or any combination of signal flags.
  HandleSignalTracker(Handle handle,
                      MojoHandleSignals signals,
                      scoped_refptr<base::SequencedTaskRunner> task_runner =
                          base::SequencedTaskRunner::GetCurrentDefault());

  HandleSignalTracker(const HandleSignalTracker&) = delete;
  HandleSignalTracker& operator=(const HandleSignalTracker&) = delete;

  ~HandleSignalTracker();

  const HandleSignalsState& last_known_state() const {
    return last_known_state_;
  }

  // Sets an optional callback to be invoked any time the tracker is notified of
  // a relevant state change.
  void set_notification_callback(NotificationCallback callback) {
    notification_callback_ = std::move(callback);
  }

 private:
  class State;

  void Arm();
  void OnNotify(MojoResult result, const HandleSignalsState& state);

  NotificationCallback notification_callback_;

  // The last known signaliing state of the handle.
  HandleSignalsState last_known_state_ = {0, 0};

  // Watches for the signal(s) to be signaled. May only be armed when
  // |low_watcher_| is not.
  SimpleWatcher high_watcher_;

  // Watches for the signal(s) to be cleared. May only be armed when
  // |high_watcher_| is not.
  SimpleWatcher low_watcher_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_SYSTEM_HANDLE_SIGNAL_TRACKER_H_
