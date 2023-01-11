// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_WTS_SESSION_CHANGE_OBSERVER_H_
#define REMOTING_HOST_WIN_WTS_SESSION_CHANGE_OBSERVER_H_

#include <stdint.h>

#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/win/message_window.h"

namespace remoting {

// Helper class to observe WM_WTSSESSION_CHANGE.
class WtsSessionChangeObserver final {
 public:
  using SessionChangeCallback =
      base::RepeatingCallback<void(uint32_t event, uint32_t session_id)>;

  WtsSessionChangeObserver();
  ~WtsSessionChangeObserver();

  // Starts observing and calls |callback| whenever a WM_WTSSESSION_CHANGE
  // message is received. The callback will be discarded once |this| is
  // destroyed.
  // Returns true if the observer is started successfully, false otherwise.
  bool Start(const SessionChangeCallback& callback);

  WtsSessionChangeObserver(const WtsSessionChangeObserver&) = delete;
  WtsSessionChangeObserver& operator=(const WtsSessionChangeObserver&) = delete;

 private:
  bool HandleMessage(UINT message,
                     WPARAM wparam,
                     LPARAM lparam,
                     LRESULT* result);

  SEQUENCE_CHECKER(sequence_checker_);

  base::win::MessageWindow message_window_
      GUARDED_BY_CONTEXT(sequence_checker_);
  SessionChangeCallback callback_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_WTS_SESSION_CHANGE_OBSERVER_H_
