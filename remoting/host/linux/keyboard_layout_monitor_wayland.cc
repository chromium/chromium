// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/keyboard_layout_monitor.h"

#include <memory>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/proto/control.pb.h"

namespace remoting {

namespace {

class KeyboardLayoutMonitorWayland : public KeyboardLayoutMonitor {
 public:
  explicit KeyboardLayoutMonitorWayland(
      base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback);

  ~KeyboardLayoutMonitorWayland() override;

  void Start() override;

 private:
  base::RepeatingCallback<void(const protocol::KeyboardLayout&)>
      layout_changed_callback_;
};

KeyboardLayoutMonitorWayland::KeyboardLayoutMonitorWayland(
    base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback)
    : layout_changed_callback_(std::move(callback)) {}

KeyboardLayoutMonitorWayland::~KeyboardLayoutMonitorWayland() = default;

void KeyboardLayoutMonitorWayland::Start() {
  NOTIMPLEMENTED();
}

}  // namespace

std::unique_ptr<KeyboardLayoutMonitor> KeyboardLayoutMonitor::Create(
    base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner) {
  return std::make_unique<KeyboardLayoutMonitorWayland>(std::move(callback));
}

}  // namespace remoting
