// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_INPUT_MONITOR_LOCAL_MOUSE_INPUT_MONITOR_X11_H_
#define REMOTING_HOST_INPUT_MONITOR_LOCAL_MOUSE_INPUT_MONITOR_X11_H_

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/host/input_monitor/local_pointer_input_monitor.h"
#include "ui/events/event.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/xinput.h"
#include "ui/gfx/x/xproto.h"

namespace remoting {

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

}  // namespace remoting

#endif  // REMOTING_HOST_INPUT_MONITOR_LOCAL_MOUSE_INPUT_MONITOR_X11_H_
