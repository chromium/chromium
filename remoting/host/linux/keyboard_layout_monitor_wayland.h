// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_KEYBOARD_LAYOUT_MONITOR_WAYLAND_H_
#define REMOTING_HOST_LINUX_KEYBOARD_LAYOUT_MONITOR_WAYLAND_H_

#include <xkbcommon/xkbcommon.h>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/host/keyboard_layout_monitor.h"
#include "remoting/host/linux/keyboard_layout_monitor_utils.h"
#include "remoting/host/linux/wayland_manager.h"
#include "remoting/proto/control.pb.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace remoting {

class KeyboardLayoutMonitorWayland : public KeyboardLayoutMonitor {
 public:
  explicit KeyboardLayoutMonitorWayland(
      base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback);
  ~KeyboardLayoutMonitorWayland() override;
  void Start() override;

 private:
  void ProcessKeymaps(XkbKeyMapUniquePtr keymap);

  // Generates a protocol layout message based on the keymap and the currently
  // active group.
  protocol::KeyboardLayout GenerateProtocolLayoutMessage();

  // Processes the modifiers of the active keyboard layout and notifies the
  // stored callbacks.
  void ProcessModifiersAndNotifyCallbacks(uint32_t group);

  void UpdateState();

  XkbKeyMapUniquePtr keymap_;
  raw_ptr<struct xkb_state> xkb_state_ = nullptr;
  xkb_layout_index_t current_group_ = XKB_LAYOUT_INVALID;
  base::RepeatingCallback<void(const protocol::KeyboardLayout&)>
      layout_changed_callback_;
  base::WeakPtrFactory<KeyboardLayoutMonitorWayland> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_KEYBOARD_LAYOUT_MONITOR_WAYLAND_H_
