// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_GNOME_LOCAL_INPUT_MONITOR_H_
#define REMOTING_HOST_LINUX_GNOME_LOCAL_INPUT_MONITOR_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "remoting/host/input_monitor/local_input_monitor.h"

namespace remoting {

// TODO(jamiewalch): Implement
class GnomeLocalInputMonitor : public LocalInputMonitor {
 public:
  void StartMonitoringForClientSession(
      base::WeakPtr<ClientSessionControl> client_session_control) override {}
  void StartMonitoring(PointerMoveCallback on_pointer_input,
                       KeyPressedCallback on_keyboard_input,
                       base::RepeatingClosure on_error) override {}
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_GNOME_LOCAL_INPUT_MONITOR_H_
