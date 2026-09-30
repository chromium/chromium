// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/thread_bound_x11_event_source.h"

#include "base/check.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/x/connection.h"

namespace remoting {

// static
void ThreadBoundX11EventSource::CreateForCurrentThread() {
  CHECK(base::CurrentUIThread::IsSet());
  // PlatformEventSource doesn't check for an existing instance, and would
  // silently replace it.
  CHECK(!ui::X11EventSource::HasInstance());
  // Deletes itself in WillDestroyCurrentMessageLoop().
  new ThreadBoundX11EventSource();
}

ThreadBoundX11EventSource::ThreadBoundX11EventSource()
    : event_source_(
          std::make_unique<ui::X11EventSource>(x11::Connection::Get())) {
  base::CurrentThread::Get()->AddDestructionObserver(this);
}

ThreadBoundX11EventSource::~ThreadBoundX11EventSource() {
  base::CurrentThread::Get()->RemoveDestructionObserver(this);
}

void ThreadBoundX11EventSource::WillDestroyCurrentMessageLoop() {
  delete this;
}

}  // namespace remoting
