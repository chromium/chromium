// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_monitor/local_mouse_input_monitor_x11.h"

#include <sys/select.h>
#include <unistd.h>

#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/host/input_monitor/local_input_monitor_x11_common.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "ui/events/event.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/xinput.h"
#include "ui/gfx/x/xproto.h"

namespace remoting {

LocalMouseInputMonitorX11::LocalMouseInputMonitorX11(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    LocalInputMonitor::PointerMoveCallback on_mouse_move)
    : core_(new Core(caller_task_runner,
                     input_task_runner,
                     std::move(on_mouse_move))) {
  core_->Start();
}

LocalMouseInputMonitorX11::~LocalMouseInputMonitorX11() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  core_->Stop();
}

LocalMouseInputMonitorX11::Core::Core(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    LocalInputMonitor::PointerMoveCallback on_mouse_move)
    : caller_task_runner_(caller_task_runner),
      input_task_runner_(input_task_runner),
      on_mouse_move_(std::move(on_mouse_move)) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
}

void LocalMouseInputMonitorX11::Core::Start() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  input_task_runner_->PostTask(FROM_HERE,
                               base::BindOnce(&Core::StartOnInputThread, this));
}

void LocalMouseInputMonitorX11::Core::Stop() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  input_task_runner_->PostTask(FROM_HERE,
                               base::BindOnce(&Core::StopOnInputThread, this));
}

LocalMouseInputMonitorX11::Core::~Core() = default;

void LocalMouseInputMonitorX11::Core::StartOnInputThread() {
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

void LocalMouseInputMonitorX11::Core::StopOnInputThread() {
  DCHECK(input_task_runner_->BelongsToCurrentThread());
  connection_->RemoveEventObserver(this);
}

void LocalMouseInputMonitorX11::Core::OnEvent(const x11::Event& event) {
  DCHECK(input_task_runner_->BelongsToCurrentThread());

  auto* raw = event.As<x11::Input::RawDeviceEvent>();
  // The X server may send unsolicited MappingNotify events without having
  // selected them.
  if (!raw) {
    return;
  }
  if (raw->opcode != x11::Input::RawDeviceEvent::RawMotion) {
    return;
  }

  connection_->QueryPointer({connection_->default_root()})
      .OnResponse(base::BindOnce(
          [](scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
             LocalInputMonitor::PointerMoveCallback on_mouse_move,
             x11::QueryPointerResponse response) {
            if (!response) {
              return;
            }
            webrtc::DesktopVector position(response->root_x, response->root_y);
            caller_task_runner->PostTask(
                FROM_HERE, base::BindOnce(on_mouse_move, position,
                                          ui::EventType::kMouseMoved));
          },
          caller_task_runner_, on_mouse_move_));
  connection_->Flush();
}

}  // namespace remoting
