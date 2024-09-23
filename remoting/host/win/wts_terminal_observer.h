// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_WTS_TERMINAL_OBSERVER_H_
#define REMOTING_HOST_WIN_WTS_TERMINAL_OBSERVER_H_

#include <windows.h>

#include <stdint.h>

namespace remoting {

// Provides callbacks for monitoring events on a WTS terminal.
class WtsTerminalObserver {
 public:
  WtsTerminalObserver(const WtsTerminalObserver&) = delete;
  WtsTerminalObserver& operator=(const WtsTerminalObserver&) = delete;

  virtual ~WtsTerminalObserver() {}

  // Called when |session_id| attaches to the console.
  virtual void OnSessionAttached(uint32_t session_id) = 0;

  // Called when a session detaches from the console.
  virtual void OnSessionDetached() = 0;

 protected:
  WtsTerminalObserver() {}
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_WTS_TERMINAL_OBSERVER_H_
