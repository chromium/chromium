// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "remoting/host/input_monitor/local_pointer_input_monitor.h"

#include <sys/select.h>
#include <unistd.h>

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
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

namespace {

// Note that this class does not detect touch input and so is named accordingly.
class LocalMouseInputMonitorX11 : public LocalPointerInputMonitor {
 public:
  LocalMouseInputMonitorX11(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      LocalInputMonitor::PointerMoveCallback on_mouse_move);

  LocalMouseInputMonitorX11(const LocalMouseInputMonitorX11&) = delete;
  LocalMouseInputMonitorX11& operator=(const LocalMouseInputMonitorX11&) =
      delete;

  ~LocalMouseInputMonitorX11() override;

 private:
  // The actual implementation resides in LocalMouseInputMonitorX11::Core class.
  class Core : public base::RefCountedThreadSafe<Core>,
               public x11::EventObserver {
   public:
    Core(scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
         scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
         LocalInputMonitor::PointerMoveCallback on_mouse_move);

    Core(const Core&) = delete;
    Core& operator=(const Core&) = delete;

    void Start();
    void Stop();

   private:
    friend class base::RefCountedThreadSafe<Core>;
    ~Core() override;

    void StartOnInputThread();
    void StopOnInputThread();

    // Called when there are pending X events.
    void OnConnectionData();

    // x11::EventObserver:
    void OnEvent(const x11::Event& event) override;

    // Task runner on which public methods of this class must be called.
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

    // Task runner on which X Window events are received.
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner_;

    // Used to send mouse event notifications.
    LocalInputMonitor::PointerMoveCallback on_mouse_move_;

    raw_ptr<x11::Connection> connection_ = nullptr;
  };

  scoped_refptr<Core> core_;

  SEQUENCE_CHECKER(sequence_checker_);
};

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
  // Let the server know the client XInput version.
  connection_->xinput().XIQueryVersion(
      {x11::Input::major_version, x11::Input::minor_version});

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
  if (!raw)
    return;
  if (raw->opcode != x11::Input::RawDeviceEvent::RawMotion)
    return;

  connection_->QueryPointer({connection_->default_root()})
      .OnResponse(base::BindOnce(
          [](scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
             LocalInputMonitor::PointerMoveCallback on_mouse_move,
             x11::QueryPointerResponse response) {
            if (!response)
              return;
            webrtc::DesktopVector position(response->root_x, response->root_y);
            caller_task_runner->PostTask(
                FROM_HERE,
                base::BindOnce(on_mouse_move, position, ui::ET_MOUSE_MOVED));
          },
          caller_task_runner_, on_mouse_move_));
  connection_->Flush();
}

}  // namespace

std::unique_ptr<LocalPointerInputMonitor> LocalPointerInputMonitor::Create(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    LocalInputMonitor::PointerMoveCallback on_mouse_move,
    base::OnceClosure disconnect_callback) {
  return std::make_unique<LocalMouseInputMonitorX11>(
      caller_task_runner, input_task_runner, std::move(on_mouse_move));
}

}  // namespace remoting
