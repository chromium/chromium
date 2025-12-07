// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_DESKTOP_EVENT_HANDLER_H_
#define REMOTING_HOST_WIN_DESKTOP_EVENT_HANDLER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/win/windows_types.h"

namespace remoting {

// A class for handling Windows events from the input desktop, even after the
// input desktop changes, e.g., due to the login screen or UAC prompt. Note that
// there is a gap after the input desktop changes, where no Windows events will
// be delivered; Delegate::OnWorkerThreadStarted() will be called after the gap
// ends. Internally a worker thread (which runs a UI message pump) is created to
// handle the events, and it will be recreated whenever the input desktop
// changes.
class DesktopEventHandler {
 public:
  // IMPORTANT: Methods will be called on an internal worker thread. The worker
  // thread always has its associated desktop set to the current input desktop,
  // such that functions like `GetCursorPos()` will always succeed if you call
  // them within the delegate method's task frame. DO NOT post tasks on the
  // worker thread's default task runner, since the thread could be destroyed
  // and re-created.
  class Delegate {
   public:
    // Note that the delegate may be destroyed on an arbitrary thread, which
    // usually happens after DesktopEventHandler is destroyed.
    virtual ~Delegate() = default;

    // Called whenever the worker thread has started. Note that the worker
    // thread may be destroyed and re-created multiple times, and this method is
    // called whenever the worker thread is re-created. There is a gap between
    // the previous thread being destroyed and the new thread being created. You
    // may run tasks such as capturing cursor info in this method to cover the
    // gap.
    virtual void OnWorkerThreadStarted() = 0;

    // Called whenever an event between [min_event, max_event] is triggered.
    // Note that this method will be called for every object in the desktop, as
    // long as the event constant is within the registered range, so you should
    // check `object_id` and ignore uninteresting events.
    virtual void OnEvent(DWORD event, LONG object_id) = 0;
  };

  DesktopEventHandler();
  ~DesktopEventHandler();

  DesktopEventHandler(const DesktopEventHandler&) = delete;
  DesktopEventHandler& operator=(const DesktopEventHandler&) = delete;

  // Starts listening for events in the range of [min_event, max_event] and
  // calls the delegate. This method takes a unique_ptr of the delegate for ease
  // of memory management, since the delegate will be called from the event
  // thread.
  // It is recommended to construct a delegate that holds a WeakPtr of the
  // caller for passing data to the caller's thread and dropping tasks after the
  // caller is destroyed.
  // For `min_event` and `max_event`, see:
  // https://learn.microsoft.com/en-us/windows/win32/winauto/event-constants
  void Start(DWORD min_event,
             DWORD max_event,
             std::unique_ptr<Delegate> delegate);

 private:
  class Core;
  scoped_refptr<Core> core_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_DESKTOP_EVENT_HANDLER_H_
