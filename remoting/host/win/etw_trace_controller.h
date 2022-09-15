// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_ETW_TRACE_CONTROLLER_H_
#define REMOTING_HOST_WIN_ETW_TRACE_CONTROLLER_H_

#include "base/sequence_checker.h"
#include "base/win/event_trace_controller.h"

namespace remoting {

// Used to control an ETW trace session for events logged by the remote access
// host.  Note: Only one instance of this class should be active at a time,
// otherwise the session started by the existing controller will be stopped.
class EtwTraceController {
 public:
  EtwTraceController();
  EtwTraceController(const EtwTraceController&) = delete;
  EtwTraceController& operator=(const EtwTraceController&) = delete;
  ~EtwTraceController();

  // Starts a trace session for events associated with the host provider GUID.
  // Returns true if this instance has started a trace session, otherwise false.
  // Note: This will attempt to stop other trace sessions (e.g. from sawbuck).
  bool Start();

  // Stops the active tracing session, if one has been started by this instance.
  void Stop();

  // The active trace session name or nullptr if there isn't an active session.
  const wchar_t* GetActiveSessionName() const;

 private:
  // Only one instance should exist at a time to prevent conflicts.
  static EtwTraceController* instance_;

  // Handles the Windows API calls needed for tracing.
  base::win::EtwTraceController controller_;

  // Used to ensure public method and destruction calls do not overlap.
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_ETW_TRACE_CONTROLLER_H_
