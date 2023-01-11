// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_KEYBOARD_LAYOUT_MONITOR_H_
#define REMOTING_HOST_KEYBOARD_LAYOUT_MONITOR_H_

#include <memory>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace remoting {

namespace protocol {
class KeyboardLayout;
}  // namespace protocol

// KeyboardLayoutMonitor implementations are responsible for monitoring the
// active keyboard layout within the CRD session on the host, and triggering a
// callback whenever it changes.
class KeyboardLayoutMonitor {
 public:
  virtual ~KeyboardLayoutMonitor() = default;

  KeyboardLayoutMonitor(const KeyboardLayoutMonitor&) = delete;
  KeyboardLayoutMonitor& operator=(const KeyboardLayoutMonitor&) = delete;

  // Starts monitoring the keyboard layout. This will generate a callback with
  // the current layout, and an additional callback whenever the layout is
  // changed. The callback is guaranteed not be be called after the
  // KeyboardLayoutMonitor is destroyed.
  virtual void Start() = 0;

  // Creates a platform-specific KeyboardLayoutMonitor.
  static std::unique_ptr<KeyboardLayoutMonitor> Create(
      base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner);

 protected:
  KeyboardLayoutMonitor() = default;
  // Physical keys to include in the keyboard map.
  static const base::span<const ui::DomCode> kSupportedKeys;
};

}  // namespace remoting

#endif  // REMOTING_HOST_KEYBOARD_LAYOUT_MONITOR_H_
