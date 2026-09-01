// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/ei_keyboard_layout_monitor.h"

#include <xkbcommon/xkbcommon.h>

#include "base/logging.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "remoting/host/linux/ei_keymap.h"
#include "remoting/host/linux/keyboard_layout_monitor_utils.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/scoped_xkb.h"

namespace remoting {

EiKeyboardLayoutMonitor::EiKeyboardLayoutMonitor(
    base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback)
    : callback_(callback) {}

EiKeyboardLayoutMonitor::~EiKeyboardLayoutMonitor() = default;

void EiKeyboardLayoutMonitor::Start() {
  started_ = true;
  callback_.Run(layout_proto_);
}

void EiKeyboardLayoutMonitor::OnKeymapChanged(EiKeymap* keymap) {
  if (keymap) {
    // Based on experimentation, there will typically be multiple keymaps: one
    // for each installed layout (in the order they are listed in the Keyboard
    // configuration applet), and a default one for US English at the end. The
    // default is present even if the only installed layout is US English. The
    // layout changes when the configured order is changed, but *not* when the
    // selected layout changes. Hence, if there are more than two layouts (ie,
    // more then one, plus the default), then it's impossible to know which is
    // active. Since the layout can be configured per-window it's probably not
    // possible to detect this solely using libei + XKB. Log a warning in this
    // situation.
    auto num_layouts = xkb_keymap_num_layouts(keymap->Get());
    if (num_layouts > 2) {
      LOG(WARNING) << "Keyboard has " << num_layouts << " layouts. Using "
                   << xkb_keymap_layout_get_name(keymap->Get(), 0)
                   << " for the client keyboard. Re-order the layouts to"
                   << " change this.";
    }
  }
  layout_proto_ =
      keymap ? keymap->GetLayoutProto() : protocol::KeyboardLayout();
  if (started_) {
    callback_.Run(layout_proto_);
  }
}

base::WeakPtr<EiKeyboardLayoutMonitor> EiKeyboardLayoutMonitor::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace remoting
