// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IPC_MOUSE_CURSOR_MONITOR_H_
#define REMOTING_HOST_IPC_MOUSE_CURSOR_MONITOR_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "remoting/protocol/mouse_cursor_monitor.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {

class DesktopSessionProxy;

// Routes MouseCursorMonitor calls through the IPC channel to the
// desktop session agent running in the desktop integration process.
class IpcMouseCursorMonitor : public protocol::MouseCursorMonitor {
 public:
  explicit IpcMouseCursorMonitor(
      scoped_refptr<DesktopSessionProxy> desktop_session_proxy);

  IpcMouseCursorMonitor(const IpcMouseCursorMonitor&) = delete;
  IpcMouseCursorMonitor& operator=(const IpcMouseCursorMonitor&) = delete;

  ~IpcMouseCursorMonitor() override;

  // MouseCursorMonitor interface.
  void Init(Callback* callback) override;
  void SetPreferredCaptureInterval(base::TimeDelta interval) override;

  // Called when the cursor shape has changed.
  void OnMouseCursor(std::unique_ptr<webrtc::MouseCursor> cursor);

  // Called when the fractional position of the mouse cursor has changed.
  void OnMouseCursorFractionalPosition(
      const protocol::FractionalCoordinate& position);

 private:
  // The callback passed to |MouseCursorMonitor::Init()|.
  raw_ptr<MouseCursorMonitor::Callback> callback_;

  // Wraps the IPC channel to the desktop session agent.
  scoped_refptr<DesktopSessionProxy> desktop_session_proxy_;

  // Used to cancel tasks pending on the capturer when it is stopped.
  base::WeakPtrFactory<IpcMouseCursorMonitor> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_IPC_MOUSE_CURSOR_MONITOR_H_
