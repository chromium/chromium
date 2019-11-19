// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_monitor/local_pointer_input_monitor.h"

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
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "ui/events/event.h"
#include "ui/gfx/x/x11.h"

namespace remoting {

namespace {

// Note that this class does not detect touch input and so is named accordingly.
class LocalMouseInputMonitorX11 : public LocalPointerInputMonitor {
 public:
  LocalMouseInputMonitorX11(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      LocalInputMonitor::PointerMoveCallback on_mouse_move);
  ~LocalMouseInputMonitorX11() override;

 private:
  // The actual implementation resides in LocalMouseInputMonitorX11::Core class.
  class Core : public base::RefCountedThreadSafe<Core> {
   public:
    Core(scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
         scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
         LocalInputMonitor::PointerMoveCallback on_mouse_move);

    void Start();
    void Stop();

   private:
    friend class base::RefCountedThreadSafe<Core>;
    ~Core();

    void StartOnInputThread();
    void StopOnInputThread();

    // Called when there are pending X events.
    void OnPendingXEvents();

    // Processes key and mouse events.
    void ProcessXEvent(xEvent* event);

    static void ProcessReply(XPointer self, XRecordInterceptData* data);

    // Task runner on which public methods of this class must be called.
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

    // Task runner on which X Window events are received.
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner_;

    // Used to send mouse event notifications.
    LocalInputMonitor::PointerMoveCallback on_mouse_move_;

    // Controls watching X events.
    std::unique_ptr<base::FileDescriptorWatcher::Controller> controller_;

    Display* display_ = nullptr;
    Display* x_record_display_ = nullptr;
    XRecordRange* x_record_range_ = nullptr;
    XRecordContext x_record_context_ = 0;

    DISALLOW_COPY_AND_ASSIGN(Core);
  };

  scoped_refptr<Core> core_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(LocalMouseInputMonitorX11);
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

LocalMouseInputMonitorX11::Core::~Core() {
  DCHECK(!display_);
  DCHECK(!x_record_display_);
  DCHECK(!x_record_range_);
  DCHECK(!x_record_context_);
}

void LocalMouseInputMonitorX11::Core::StartOnInputThread() {
  DCHECK(input_task_runner_->BelongsToCurrentThread());
  DCHECK(!display_);
  DCHECK(!x_record_display_);
  DCHECK(!x_record_range_);
  DCHECK(!x_record_context_);

  // TODO(jamiewalch): We should pass the display in. At that point, since
  // XRecord needs a private connection to the X Server for its data channel
  // and both channels are used from a separate thread, we'll need to duplicate
  // them with something like the following:
  //   XOpenDisplay(DisplayString(display));
  display_ = XOpenDisplay(nullptr);
  x_record_display_ = XOpenDisplay(nullptr);
  if (!display_ || !x_record_display_) {
    LOG(ERROR) << "Couldn't open X display";
    return;
  }

  int xr_opcode, xr_event, xr_error;
  if (!XQueryExtension(display_, "RECORD", &xr_opcode, &xr_event, &xr_error)) {
    LOG(ERROR) << "X Record extension not available.";
    return;
  }

  x_record_range_ = XRecordAllocRange();
  if (!x_record_range_) {
    LOG(ERROR) << "XRecordAllocRange failed.";
    return;
  }
  x_record_range_->device_events.first = MotionNotify;
  x_record_range_->device_events.last = MotionNotify;
  XRecordClientSpec client_spec = XRecordAllClients;

  x_record_context_ = XRecordCreateContext(x_record_display_, 0, &client_spec,
                                           1, &x_record_range_, 1);
  if (!x_record_context_) {
    LOG(ERROR) << "XRecordCreateContext failed.";
    return;
  }

  if (!XRecordEnableContextAsync(x_record_display_, x_record_context_,
                                 &Core::ProcessReply,
                                 reinterpret_cast<XPointer>(this))) {
    LOG(ERROR) << "XRecordEnableContextAsync failed.";
    return;
  }

  // Register OnPendingXEvents() to be called every time there is
  // something to read from |x_record_display_|.
  controller_ = base::FileDescriptorWatcher::WatchReadable(
      ConnectionNumber(x_record_display_),
      base::BindRepeating(&Core::OnPendingXEvents, base::Unretained(this)));

  // Fetch pending events if any.
  while (XPending(x_record_display_)) {
    XEvent ev;
    XNextEvent(x_record_display_, &ev);
  }
}

void LocalMouseInputMonitorX11::Core::StopOnInputThread() {
  DCHECK(input_task_runner_->BelongsToCurrentThread());

  // Context must be disabled via the control channel because we can't send
  // any X protocol traffic over the data channel while it's recording.
  if (x_record_context_) {
    XRecordDisableContext(display_, x_record_context_);
    XFlush(display_);
  }

  controller_.reset();

  if (x_record_range_) {
    XFree(x_record_range_);
    x_record_range_ = nullptr;
  }
  if (x_record_context_) {
    XRecordFreeContext(x_record_display_, x_record_context_);
    x_record_context_ = 0;
  }
  if (x_record_display_) {
    XCloseDisplay(x_record_display_);
    x_record_display_ = nullptr;
  }
  if (display_) {
    XCloseDisplay(display_);
    display_ = nullptr;
  }
}

void LocalMouseInputMonitorX11::Core::OnPendingXEvents() {
  DCHECK(input_task_runner_->BelongsToCurrentThread());

  // Fetch pending events if any.
  while (XPending(x_record_display_)) {
    XEvent ev;
    XNextEvent(x_record_display_, &ev);
  }
}

void LocalMouseInputMonitorX11::Core::ProcessXEvent(xEvent* event) {
  DCHECK(input_task_runner_->BelongsToCurrentThread());
  if (event->u.u.type != MotionNotify) {
    return;
  }

  webrtc::DesktopVector position(event->u.keyButtonPointer.rootX,
                                 event->u.keyButtonPointer.rootY);
  caller_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(on_mouse_move_, position, ui::ET_MOUSE_MOVED));
}

// static
void LocalMouseInputMonitorX11::Core::ProcessReply(XPointer self,
                                                   XRecordInterceptData* data) {
  if (data->category == XRecordFromServer) {
    xEvent* event = reinterpret_cast<xEvent*>(data->data);
    reinterpret_cast<Core*>(self)->ProcessXEvent(event);
  }
  XRecordFreeData(data);
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
