// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IPC_KEYBOARD_LAYOUT_MONITOR_H_
#define REMOTING_HOST_IPC_KEYBOARD_LAYOUT_MONITOR_H_

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "remoting/host/keyboard_layout_monitor.h"

namespace remoting {

class DesktopSessionProxy;

// KeyboardLayoutMonitor implementations are responsible for monitoring the
// active keyboard layout within the CRD session on the host, and triggering a
// callback whenever it changes.
class IpcKeyboardLayoutMonitor : public KeyboardLayoutMonitor {
 public:
  IpcKeyboardLayoutMonitor(
      base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback,
      scoped_refptr<DesktopSessionProxy> desktop_session_proxy);
  ~IpcKeyboardLayoutMonitor() override;

  IpcKeyboardLayoutMonitor(const IpcKeyboardLayoutMonitor&) = delete;
  IpcKeyboardLayoutMonitor& operator=(const IpcKeyboardLayoutMonitor&) = delete;

  // KeyboardLayoutMonitor implementation.
  void Start() override;

  // Called when KeyboardChanged IPC message is received.
  void OnKeyboardChanged(const protocol::KeyboardLayout& layout);

 private:
  base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback_;
  scoped_refptr<DesktopSessionProxy> desktop_session_proxy_;
  base::WeakPtrFactory<IpcKeyboardLayoutMonitor> weak_ptr_factory_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_IPC_KEYBOARD_LAYOUT_MONITOR_H_
