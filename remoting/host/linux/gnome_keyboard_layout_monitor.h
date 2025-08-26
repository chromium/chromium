// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_GNOME_KEYBOARD_LAYOUT_MONITOR_H_
#define REMOTING_HOST_LINUX_GNOME_KEYBOARD_LAYOUT_MONITOR_H_

#include <xkbcommon/xkbcommon.h>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "remoting/host/keyboard_layout_monitor.h"
#include "remoting/proto/control.pb.h"

namespace remoting {

class EiKeymap;

class GnomeKeyboardLayoutMonitor : public KeyboardLayoutMonitor {
 public:
  explicit GnomeKeyboardLayoutMonitor(
      base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback);
  ~GnomeKeyboardLayoutMonitor() override;

  // KeyboardLayoutMonitor interface
  void Start() override;

  // Send the new layout to the client. If `keymap` is nullptr then send an
  // empty layout.
  void OnKeymapChanged(EiKeymap* keymap);

  base::WeakPtr<GnomeKeyboardLayoutMonitor> GetWeakPtr();

 private:
  bool started_ = false;
  protocol::KeyboardLayout layout_proto_;
  base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback_;

  base::WeakPtrFactory<GnomeKeyboardLayoutMonitor> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_GNOME_KEYBOARD_LAYOUT_MONITOR_H_
