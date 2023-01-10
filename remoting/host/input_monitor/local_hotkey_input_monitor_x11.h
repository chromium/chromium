// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_INPUT_MONITOR_LOCAL_HOTKEY_INPUT_MONITOR_X11_H_
#define REMOTING_HOST_INPUT_MONITOR_LOCAL_HOTKEY_INPUT_MONITOR_X11_H_

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/host/input_monitor/local_hotkey_input_monitor.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/future.h"

namespace remoting {

class LocalHotkeyInputMonitorX11 : public LocalHotkeyInputMonitor {
 public:
  LocalHotkeyInputMonitorX11(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      base::OnceClosure disconnect_callback);
  LocalHotkeyInputMonitorX11(const LocalHotkeyInputMonitorX11&) = delete;
  LocalHotkeyInputMonitorX11& operator=(const LocalHotkeyInputMonitorX11&) =
      delete;
  ~LocalHotkeyInputMonitorX11() override;

 private:
  // The implementation resides in LocalHotkeyInputMonitorX11::Core class.
  class Core : public base::RefCountedThreadSafe<Core>,
               public x11::EventObserver {
   public:
    Core(scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
         scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
         base::OnceClosure disconnect_callback);
    Core(const Core&) = delete;
    Core& operator=(const Core&) = delete;
    void Start();
    void Stop();

   private:
    friend class base::RefCountedThreadSafe<Core>;
    ~Core() override;
    void StartOnInputThread();
    void StopOnInputThread();
    // x11::EventObserver:
    void OnEvent(const x11::Event& event) override;
    // Task runner on which public methods of this class must be called.
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;
    // Task runner on which X Window events are received.
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner_;
    // Used to send session disconnect requests.
    base::OnceClosure disconnect_callback_;
    // True when Alt is pressed.
    bool alt_pressed_ = false;
    // True when Ctrl is pressed.
    bool ctrl_pressed_ = false;
    raw_ptr<x11::Connection> connection_ = nullptr;
  };
  scoped_refptr<Core> core_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace remoting

#endif  // REMOTING_HOST_INPUT_MONITOR_LOCAL_HOTKEY_INPUT_MONITOR_X11_H_
