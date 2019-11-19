// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_monitor/local_input_monitor.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/input_monitor/local_hotkey_input_monitor.h"
#include "remoting/host/input_monitor/local_keyboard_input_monitor.h"
#include "remoting/host/input_monitor/local_pointer_input_monitor.h"

namespace remoting {
namespace {

class LocalInputMonitorImpl : public LocalInputMonitor {
 public:
  LocalInputMonitorImpl(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);
  ~LocalInputMonitorImpl() override;

  // LocalInputMonitor implementation.
  void StartMonitoringForClientSession(
      base::WeakPtr<ClientSessionControl> client_session_control) override;
  void StartMonitoring(PointerMoveCallback on_pointer_input,
                       KeyPressedCallback on_keyboard_input,
                       base::RepeatingClosure on_error) override;

 private:
  void OnMonitoringStarted();

  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> input_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  std::unique_ptr<LocalHotkeyInputMonitor> hotkey_input_monitor_;
  std::unique_ptr<LocalKeyboardInputMonitor> keyboard_input_monitor_;
  std::unique_ptr<LocalPointerInputMonitor> pointer_input_monitor_;

  // Indicates whether the instance is actively monitoring local input.
  bool monitoring_ = false;

  DISALLOW_COPY_AND_ASSIGN(LocalInputMonitorImpl);
};

LocalInputMonitorImpl::LocalInputMonitorImpl(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : caller_task_runner_(caller_task_runner),
      input_task_runner_(input_task_runner),
      ui_task_runner_(ui_task_runner) {}

LocalInputMonitorImpl::~LocalInputMonitorImpl() {
  // LocalInputMonitor sub-classes expect to be torn down on the caller thread.
  if (!caller_task_runner_->BelongsToCurrentThread()) {
    if (hotkey_input_monitor_)
      caller_task_runner_->DeleteSoon(FROM_HERE,
                                      hotkey_input_monitor_.release());
    if (keyboard_input_monitor_)
      caller_task_runner_->DeleteSoon(FROM_HERE,
                                      keyboard_input_monitor_.release());
    if (pointer_input_monitor_)
      caller_task_runner_->DeleteSoon(FROM_HERE,
                                      pointer_input_monitor_.release());
  }
  caller_task_runner_ = nullptr;
}

void LocalInputMonitorImpl::StartMonitoringForClientSession(
    base::WeakPtr<ClientSessionControl> client_session_control) {
  DCHECK(!monitoring_);

  hotkey_input_monitor_ = LocalHotkeyInputMonitor::Create(
      caller_task_runner_, input_task_runner_, ui_task_runner_,
      base::BindOnce(&ClientSessionControl::DisconnectSession,
                     client_session_control, protocol::OK));

  pointer_input_monitor_ = LocalPointerInputMonitor::Create(
      caller_task_runner_, input_task_runner_, ui_task_runner_,
      base::BindRepeating(&ClientSessionControl::OnLocalPointerMoved,
                          client_session_control),
      base::BindOnce(&ClientSessionControl::DisconnectSession,
                     client_session_control, protocol::OK));

  keyboard_input_monitor_ = LocalKeyboardInputMonitor::Create(
      caller_task_runner_, input_task_runner_, ui_task_runner_,
      base::BindRepeating(&ClientSessionControl::OnLocalKeyPressed,
                          client_session_control),
      base::BindOnce(&ClientSessionControl::DisconnectSession,
                     client_session_control, protocol::OK));

  OnMonitoringStarted();
}

void LocalInputMonitorImpl::StartMonitoring(
    PointerMoveCallback on_pointer_input,
    KeyPressedCallback on_keyboard_input,
    base::RepeatingClosure on_error) {
  DCHECK(!monitoring_);
  DCHECK(on_error);
  DCHECK(on_pointer_input || on_keyboard_input);

  if (on_pointer_input) {
    pointer_input_monitor_ = LocalPointerInputMonitor::Create(
        caller_task_runner_, input_task_runner_, ui_task_runner_,
        std::move(on_pointer_input), base::BindOnce(on_error));
  }
  if (on_keyboard_input) {
    keyboard_input_monitor_ = LocalKeyboardInputMonitor::Create(
        caller_task_runner_, input_task_runner_, ui_task_runner_,
        std::move(on_keyboard_input), base::BindOnce(on_error));
  }

  OnMonitoringStarted();
}

void LocalInputMonitorImpl::OnMonitoringStarted() {
  monitoring_ = true;

  // We don't need these after we've provided them to the per-input monitors.
  // Note: we want to keep |caller_task_runner_| as we use it to ensure the
  // underlying input monitors are released on the appropriate thread.
  input_task_runner_ = nullptr;
  ui_task_runner_ = nullptr;
}

}  // namespace

std::unique_ptr<LocalInputMonitor> LocalInputMonitor::Create(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  return std::make_unique<LocalInputMonitorImpl>(
      caller_task_runner, input_task_runner, ui_task_runner);
}

}  // namespace remoting
