// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_monitor/local_hotkey_input_monitor_x11.h"

#include <sys/select.h>
#include <unistd.h>

#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "remoting/host/input_monitor/local_input_monitor_x11_common.h"
#include "ui/gfx/x/keysyms/keysyms.h"
#include "ui/gfx/x/xinput.h"

namespace remoting {

LocalHotkeyInputMonitorX11::LocalHotkeyInputMonitorX11(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    base::OnceClosure disconnect_callback)
    : core_(new Core(caller_task_runner,
                     input_task_runner,
                     std::move(disconnect_callback))) {
  core_->Start();
}

LocalHotkeyInputMonitorX11::~LocalHotkeyInputMonitorX11() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  core_->Stop();
}

LocalHotkeyInputMonitorX11::Core::Core(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    base::OnceClosure disconnect_callback)
    : caller_task_runner_(caller_task_runner),
      input_task_runner_(input_task_runner),
      disconnect_callback_(std::move(disconnect_callback)) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(disconnect_callback_);
}

void LocalHotkeyInputMonitorX11::Core::Start() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  input_task_runner_->PostTask(FROM_HERE,
                               base::BindOnce(&Core::StartOnInputThread, this));
}

void LocalHotkeyInputMonitorX11::Core::Stop() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  input_task_runner_->PostTask(FROM_HERE,
                               base::BindOnce(&Core::StopOnInputThread, this));
}

LocalHotkeyInputMonitorX11::Core::~Core() = default;

void LocalHotkeyInputMonitorX11::Core::StartOnInputThread() {
  DCHECK(input_task_runner_->BelongsToCurrentThread());
  DCHECK(!connection_);

  connection_ = x11::Connection::Get();
  connection_->AddEventObserver(this);

  if (!connection_->xinput().present()) {
    LOG(ERROR) << "X Input extension not available.";
    return;
  }

  auto mask = CommonXIEventMaskForRootWindow();
  connection_->xinput().XISelectEvents(
      {connection_->default_root(),
       {{x11::Input::DeviceId::AllMaster, {mask}}}});
  connection_->Flush();
}

void LocalHotkeyInputMonitorX11::Core::StopOnInputThread() {
  DCHECK(input_task_runner_->BelongsToCurrentThread());
  connection_->RemoveEventObserver(this);
}

void LocalHotkeyInputMonitorX11::Core::OnEvent(const x11::Event& event) {
  DCHECK(input_task_runner_->BelongsToCurrentThread());

  // Ignore input if we've already initiated a disconnect.
  if (!disconnect_callback_) {
    return;
  }

  const auto* raw = event.As<x11::Input::RawDeviceEvent>();
  // The X server may send unsolicited MappingNotify events without having
  // selected them.
  if (!raw) {
    return;
  }
  if (raw->opcode != x11::Input::RawDeviceEvent::RawKeyPress &&
      raw->opcode != x11::Input::RawDeviceEvent::RawKeyRelease) {
    return;
  }

  const bool down = raw->opcode == x11::Input::RawDeviceEvent::RawKeyPress;
  const auto key_sym =
      connection_->KeycodeToKeysym(static_cast<x11::KeyCode>(raw->detail), 0);

  if (key_sym == XK_Control_L || key_sym == XK_Control_R) {
    ctrl_pressed_ = down;
  } else if (key_sym == XK_Alt_L || key_sym == XK_Alt_R) {
    alt_pressed_ = down;
  } else if (key_sym == XK_Escape && down && alt_pressed_ && ctrl_pressed_) {
    caller_task_runner_->PostTask(FROM_HERE, std::move(disconnect_callback_));
  }
}

}  // namespace remoting
