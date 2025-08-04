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
  using CallbackSignature = void(const DesktopDisplayInfo&);
  using Callback = base::RepeatingCallback<CallbackSignature>;

  virtual ~DesktopDisplayInfoMonitor() = default;

  // Begins continuous monitoring for changes. Any changes to the monitor layout
  // will be reported to the registered callbacks.
  virtual void Start() = 0;

  // Queries the OS immediately for the current monitor layout and reports any
  // changed display info to the registered callbacks. If this instance is
  // associated with only one DesktopCapturerProxy, this method could be used to
  // query the display info after each captured frame. If there are multiple
  // capturers all linked to this instance, it doesn't make sense to query after
  // every captured frame. So Start() should be called instead, and subsequent
  // calls to QueryDisplayInfo() will have no effect.
  virtual void QueryDisplayInfo() = 0;

  // Adds a callback to be notified of display-info changes. Callbacks must not
  // be added after calling Start() or QueryDisplayInfo(). Implementations do
  // not return a base::CallbackListSubscription, so |callback| must either
  // outlive this object, or be bound to a suitable WeakPtr.
  virtual void AddCallback(Callback callback) = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_DISPLAY_INFO_MONITOR_H_
