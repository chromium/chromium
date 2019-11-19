// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_INPUT_MONITOR_LOCAL_INPUT_MONITOR_H_
#define REMOTING_HOST_INPUT_MONITOR_LOCAL_INPUT_MONITOR_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "ui/events/event.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace webrtc {
class DesktopVector;
}  // namespace webrtc

namespace remoting {

class ClientSessionControl;

// Monitors local input and sends notifications for mouse and touch movements
// and keyboard shortcuts when they are detected.
class LocalInputMonitor {
 public:
  using PointerMoveCallback =
      base::RepeatingCallback<void(const webrtc::DesktopVector&,
                                   ui::EventType)>;
  using KeyPressedCallback = base::RepeatingCallback<void(uint32_t)>;

  virtual ~LocalInputMonitor() = default;

  // Creates a platform-specific instance of LocalInputMonitor.
  // |caller_task_runner| is used for all callbacks and notifications.
  static std::unique_ptr<LocalInputMonitor> Create(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

  // Start monitoring and notify using |client_session_control|.  In this mode
  // the LocalInputMonitor will listen for session disconnect hotkeys and mouse
  // and keyboard events (and touch, on some platforms) for input filtering.
  virtual void StartMonitoringForClientSession(
      base::WeakPtr<ClientSessionControl> client_session_control) = 0;

  // Start monitoring and notify using the callbacks specified.
  // Monitors are only started if the respective callback is provided.  This
  // means that callbacks are optional, but at least one must be valid.
  // |on_pointer_input| is called for each mouse or touch movement detected.
  // |on_keyboard_input| is called for each keypress detected.
  // |on_error| is called if any of the child input monitors fail.
  virtual void StartMonitoring(PointerMoveCallback on_pointer_input,
                               KeyPressedCallback on_keyboard_input,
                               base::RepeatingClosure on_error) = 0;

 protected:
  LocalInputMonitor() = default;
};

}  // namespace remoting

#endif  // REMOTING_HOST_INPUT_MONITOR_LOCAL_INPUT_MONITOR_H_
