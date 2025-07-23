// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_GNOME_KEYBOARD_LAYOUT_MONITOR_H_
#define REMOTING_HOST_LINUX_GNOME_KEYBOARD_LAYOUT_MONITOR_H_

#include "remoting/host/keyboard_layout_monitor.h"

namespace remoting {

class GnomeKeyboardLayoutMonitor : public KeyboardLayoutMonitor {
 public:
  ~GnomeKeyboardLayoutMonitor() override = default;
  void Start() override {
    // TODO(jamiewalch): Implement
  }
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_GNOME_KEYBOARD_LAYOUT_MONITOR_H_
