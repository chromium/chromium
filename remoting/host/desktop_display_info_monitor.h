// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_DISPLAY_INFO_MONITOR_H_
#define REMOTING_HOST_DESKTOP_DISPLAY_INFO_MONITOR_H_

#include "base/functional/callback.h"
#include "remoting/host/desktop_display_info.h"

namespace remoting {

// An interface that monitors the OS for any changes to the multi-monitor
// display configuration, and reports any changes to the registered callbacks.
class DesktopDisplayInfoMonitor {
 public:
  virtual ~DesktopDisplayInfoMonitor() = default;

  // Begins continuous monitoring for changes. Any changes to the monitor layout
  // will be reported to the registered callbacks.
  // No-op if the monitor is already started.
  virtual void Start() = 0;

  // Returns whether the monitor has started.
  virtual bool IsStarted() const = 0;

  // Returns the latest known display info, or nullptr if it hasn't been fetched
  // yet.
  virtual const DesktopDisplayInfo* GetLatestDisplayInfo() const = 0;

  // Adds a callback to be notified of display-info changes or the first
  // available display info after Start() is called. Callbacks added after
  // calling Start() will NOT be called until it changes. Implementations do not
  // return a base::CallbackListSubscription, so |callback| must either outlive
  // this object, or be bound to a suitable WeakPtr.
  virtual void AddCallback(base::RepeatingClosure callback) = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_DISPLAY_INFO_MONITOR_H_
