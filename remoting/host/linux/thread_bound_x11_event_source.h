// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_THREAD_BOUND_X11_EVENT_SOURCE_H_
#define REMOTING_HOST_LINUX_THREAD_BOUND_X11_EVENT_SOURCE_H_

#include <memory>

#include "base/task/current_thread.h"

namespace ui {
class X11EventSource;
}  // namespace ui

namespace remoting {

// Owns a ui::X11EventSource for the current thread, so that X events on the
// thread's x11::Connection are dispatched. The instance deletes itself right
// before the thread's task execution environment is destroyed, while the
// message pump is still alive. This is required because X11EventSource
// registers an fd watch with the message pump (a GSource on the pump's
// GMainContext, or an FdWatchController on non-GLib pumps), which must be
// removed before the message pump is destroyed. For the same reason,
// base::SequenceLocalStorageSlot can't be used here, since its values are
// destroyed after the message pump.
class ThreadBoundX11EventSource final
    : public base::CurrentThread::DestructionObserver {
 public:
  // Creates an instance for the current thread, which must have a UI message
  // pump and must not already have an X11EventSource.
  static void CreateForCurrentThread();

  ThreadBoundX11EventSource(const ThreadBoundX11EventSource&) = delete;
  ThreadBoundX11EventSource& operator=(const ThreadBoundX11EventSource&) =
      delete;

 private:
  ThreadBoundX11EventSource();
  ~ThreadBoundX11EventSource() override;

  // base::CurrentThread::DestructionObserver:
  void WillDestroyCurrentMessageLoop() override;

  std::unique_ptr<ui::X11EventSource> event_source_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_THREAD_BOUND_X11_EVENT_SOURCE_H_
