// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/ei_keyboard_layout_monitor.h"

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
