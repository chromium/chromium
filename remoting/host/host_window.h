// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_WINDOW_H_
#define REMOTING_HOST_HOST_WINDOW_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"

namespace remoting {

class ClientSessionControl;
class LocalInputMonitor;

class HostWindow {
 public:
  HostWindow(const HostWindow&) = delete;
  HostWindow& operator=(const HostWindow&) = delete;

  virtual ~HostWindow();

  // Creates a platform-specific instance of the continue window.
  static std::unique_ptr<HostWindow> CreateContinueWindow();

  // Creates a platform-specific instance of the disconnect window.
  static std::unique_ptr<HostWindow> CreateDisconnectWindow();

  // Creates a platform-specific instance of the disconnect window which hides
  // after a period of inactivity on the local desktop and shows the window when
  // local input is observed.
  static std::unique_ptr<HostWindow> CreateAutoHidingDisconnectWindow(
      std::unique_ptr<LocalInputMonitor> local_input_monitor);

  // Starts the UI state machine. |client_session_control| will be used to
  // notify the caller about the local user's actions.
  virtual void Start(
      const base::WeakPtr<ClientSessionControl>& client_session_control) = 0;

 protected:
  // Let |HostWindowProxy| to call DetachFromSequence() when passing an instance
  // of |HostWindow| to a different sequence.
  friend class HostWindowProxy;

  HostWindow() {}

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_WINDOW_H_
