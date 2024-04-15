// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/wts_session_change_observer.h"

#include <windows.h>

#include <winuser.h>
#include <wtsapi32.h>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/sequence_checker.h"

namespace remoting {

WtsSessionChangeObserver::WtsSessionChangeObserver() = default;

WtsSessionChangeObserver::~WtsSessionChangeObserver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool WtsSessionChangeObserver::Start(const SessionChangeCallback& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!message_window_.Create(base::BindRepeating(
          &WtsSessionChangeObserver::HandleMessage, base::Unretained(this)))) {
    PLOG(ERROR) << "Failed to create the WTS session notification window";
    return false;
  }

  if (!WTSRegisterSessionNotification(message_window_.hwnd(),
                                      NOTIFY_FOR_THIS_SESSION)) {
    PLOG(ERROR) << "Failed to register WTS session notification";
    return false;
  }
  callback_ = callback;
  return true;
}

bool WtsSessionChangeObserver::HandleMessage(UINT message,
                                             WPARAM wparam,
                                             LPARAM lparam,
                                             LRESULT* result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (message == WM_WTSSESSION_CHANGE) {
    callback_.Run(wparam, lparam);
    *result = 0;
    return true;
  }

  return false;
}

}  // namespace remoting
