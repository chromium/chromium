// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/one_shot_timeout_monitor.h"

#include "base/trace_event/trace_event.h"

namespace content {

OneShotTimeoutMonitor::OneShotTimeoutMonitor(TimeoutHandler timeout_handler,
                                             base::TimeDelta delay)
    : timeout_handler_(std::move(timeout_handler)), delay_(delay) {
  DCHECK(!timeout_handler_.is_null());
  Start();
}

OneShotTimeoutMonitor::~OneShotTimeoutMonitor() {
  if (timeout_timer_.IsRunning())
    timeout_timer_.Stop();
}

void OneShotTimeoutMonitor::Start() {
  TRACE_EVENT_ASYNC_BEGIN0("renderer_host", "OneShotTimeoutMonitor", this);
  TRACE_EVENT_INSTANT0("renderer_host", "OneShotTimeoutMonitor::Start",
                       TRACE_EVENT_SCOPE_THREAD);

  timeout_timer_.Start(FROM_HERE, delay_, this,
                       &OneShotTimeoutMonitor::TimedOut);
}

void OneShotTimeoutMonitor::TimedOut() {
  TRACE_EVENT_ASYNC_END1("renderer_host", "OneShotTimeoutMonitor", this,
                         "result", "timed_out");
  TRACE_EVENT0("renderer_host", "OneShotTimeoutMonitor::TimeOutHandler");
  std::move(timeout_handler_).Run();
}

}  // namespace content
