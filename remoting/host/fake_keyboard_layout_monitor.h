// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FAKE_KEYBOARD_LAYOUT_MONITOR_H_
#define REMOTING_HOST_FAKE_KEYBOARD_LAYOUT_MONITOR_H_

#include "remoting/host/keyboard_layout_monitor.h"

namespace remoting {

// Dummy KeyboardLayoutMonitor than never calls the provided callback.
class FakeKeyboardLayoutMonitor : public KeyboardLayoutMonitor {
 public:
  FakeKeyboardLayoutMonitor();
  ~FakeKeyboardLayoutMonitor() override;

  void Start() override;
};

}  // namespace remoting

#endif  // REMOTING_HOST_FAKE_KEYBOARD_LAYOUT_MONITOR_H_
