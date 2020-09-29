// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_monitor/local_hotkey_input_monitor.h"

#include <sys/select.h>
#include <unistd.h>

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_descriptor_watcher_posix.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"
#include "ui/events/devices/x11/xinput_util.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/keysyms/keysyms.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/xinput.h"

namespace remoting {

namespace {

class LocalHotkeyInputMonitorX11 : public LocalHotkeyInputMonitor {
 public:
  LocalHotkeyInputMonitorX11(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      base::OnceClosure disconnect_callback);
  ~LocalHotkeyInputMonitorX11() override;

 private:
  // The implementation resides in LocalHotkeyInputMonitorX11::Core class.
  class Core : public base::RefCountedThreadSafe<Core>,
               public x11::Connection::Delegate {
   public:
    Core(scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
         scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
         base::OnceClosure disconnect_callback);

    void Start();
    void Stop();

   private:
    friend class base::RefCountedThreadSafe<Core>;
    ~Core() override;

    void StartOnInputThread();
    void StopOnInputThread();

    // Called when there are pending X events.
    void OnConnectionData();

    // x11::Connection::Delegate:
    bool ShouldContinueStream() const override;
    void DispatchXEvent(x11::Event* event) override;

    // Task runner on which public methods of this class must be called.
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

    // Task runner on which X Window events are received.
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner_;

    // Used to send session disconnect requests.
    base::OnceClosure disconnect_callback_;

    // Controls watching X events.
    std::unique_ptr<base::FileDescriptorWatcher::Controller> controller_;

    // True when Alt is pressed.
    bool alt_pressed_ = false;

    // True when Ctrl is pressed.
    bool ctrl_pressed_ = false;

    std::unique_ptr<x11::Connection> connection_;

    DISALLOW_COPY_AND_ASSIGN(Core);
  };

  scoped_refptr<Core> core_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(LocalHotkeyInputMonitorX11);
};

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

LocalHotkeyInputMonitorX11::Core::~Core() {
  DCHECK(!connection_);
}

void LocalHotkeyInputMonitorX11::Core::StartOnInputThread() {
  DCHECK(input_task_runner_->BelongsToCurrentThread());
  DCHECK(!connection_);

  // TODO(jamiewalch): We should pass the connection in.
  connection_ = std::make_unique<x11::Connection>();

  if (!connection_->xinput().present()) {
    LOG(ERROR) << "X Record extension not available.";
    return;
  }
  // Let the server know the client XInput version.
  connection_->xinput().XIQueryVersion(
      {x11::Input::major_version, x11::Input::minor_version});

  x11::Input::XIEventMask mask;
  ui::SetXinputMask(&mask, x11::Input::RawDeviceEvent::RawKeyPress);
  ui::SetXinputMask(&mask, x11::Input::RawDeviceEvent::RawKeyRelease);
  connection_->xinput().XISelectEvents(
      {connection_->default_root(),
       {{x11::Input::DeviceId::AllMaster, {mask}}}});
  connection_->Flush();

  // Register OnConnectionData() to be called every time there is
  // something to read from |connection_|.
  controller_ = base::FileDescriptorWatcher::WatchReadable(
      connection_->GetFd(),
      base::BindRepeating(&Core::OnConnectionData, base::Unretained(this)));

  // Fetch pending events if any.
  OnConnectionData();
}

void LocalHotkeyInputMonitorX11::Core::StopOnInputThread() {
  DCHECK(input_task_runner_->BelongsToCurrentThread());
  controller_.reset();
  connection_.reset();
}

void LocalHotkeyInputMonitorX11::Core::OnConnectionData() {
  DCHECK(input_task_runner_->BelongsToCurrentThread());
  connection_->Dispatch(this);
}

bool LocalHotkeyInputMonitorX11::Core::ShouldContinueStream() const {
  return true;
}

void LocalHotkeyInputMonitorX11::Core::DispatchXEvent(x11::Event* event) {
  DCHECK(input_task_runner_->BelongsToCurrentThread());

  // Ignore input if we've already initiated a disconnect.
  if (!disconnect_callback_) {
    return;
  }

  auto* raw = event->As<x11::Input::RawDeviceEvent>();
  DCHECK(raw);
  DCHECK(raw->opcode == x11::Input::RawDeviceEvent::RawKeyPress ||
         raw->opcode == x11::Input::RawDeviceEvent::RawKeyRelease);

  bool down = raw->opcode == x11::Input::RawDeviceEvent::RawKeyPress;
  auto key_sym =
      static_cast<uint32_t>(connection_->KeycodeToKeysym(raw->detail, 0));

  if (key_sym == XK_Control_L || key_sym == XK_Control_R)
    ctrl_pressed_ = down;
  else if (key_sym == XK_Alt_L || key_sym == XK_Alt_R)
    alt_pressed_ = down;
  else if (key_sym == XK_Escape && down && alt_pressed_ && ctrl_pressed_)
    caller_task_runner_->PostTask(FROM_HERE, std::move(disconnect_callback_));
}

}  // namespace

std::unique_ptr<LocalHotkeyInputMonitor> LocalHotkeyInputMonitor::Create(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    base::OnceClosure disconnect_callback) {
  return std::make_unique<LocalHotkeyInputMonitorX11>(
      caller_task_runner, input_task_runner, std::move(disconnect_callback));
}

}  // namespace remoting
