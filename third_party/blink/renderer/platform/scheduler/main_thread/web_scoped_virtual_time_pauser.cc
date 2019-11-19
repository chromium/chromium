// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/scheduler/web_scoped_virtual_time_pauser.h"

#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"

namespace blink {

WebScopedVirtualTimePauser::WebScopedVirtualTimePauser()
    : scheduler_(nullptr) {}

WebScopedVirtualTimePauser::WebScopedVirtualTimePauser(
    scheduler::MainThreadSchedulerImpl* scheduler,
    VirtualTaskDuration duration,
    const WebString& name)
    : duration_(duration),
      scheduler_(scheduler),
      debug_name_(name),
      trace_id_(reinterpret_cast<intptr_t>(this)) {}

WebScopedVirtualTimePauser::~WebScopedVirtualTimePauser() {
  if (paused_ && scheduler_)
    DecrementVirtualTimePauseCount();
}

WebScopedVirtualTimePauser::WebScopedVirtualTimePauser(
    WebScopedVirtualTimePauser&& other) {
  virtual_time_when_paused_ = other.virtual_time_when_paused_;
  paused_ = other.paused_;
  duration_ = other.duration_;
  scheduler_ = std::move(other.scheduler_);
  debug_name_ = std::move(other.debug_name_);
  other.scheduler_ = nullptr;
  trace_id_ = other.trace_id_;
}

WebScopedVirtualTimePauser& WebScopedVirtualTimePauser::operator=(
    WebScopedVirtualTimePauser&& other) {
  if (scheduler_ && paused_)
    DecrementVirtualTimePauseCount();
  virtual_time_when_paused_ = other.virtual_time_when_paused_;
  paused_ = other.paused_;
  duration_ = other.duration_;
  scheduler_ = std::move(other.scheduler_);
  debug_name_ = std::move(other.debug_name_);
  trace_id_ = other.trace_id_;
  other.scheduler_ = nullptr;
  return *this;
}

void WebScopedVirtualTimePauser::PauseVirtualTime() {
  if (paused_ || !scheduler_)
    return;

  paused_ = true;
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      "renderer.scheduler", "WebScopedVirtualTimePauser::PauseVirtualTime",
      trace_id_, "name", debug_name_.Latin1());
  virtual_time_when_paused_ = scheduler_->IncrementVirtualTimePauseCount();
}

void WebScopedVirtualTimePauser::UnpauseVirtualTime() {
  if (!paused_ || !scheduler_)
    return;

  paused_ = false;
  DecrementVirtualTimePauseCount();
}

void WebScopedVirtualTimePauser::DecrementVirtualTimePauseCount() {
  scheduler_->DecrementVirtualTimePauseCount();
  if (duration_ == VirtualTaskDuration::kNonInstant) {
    scheduler_->MaybeAdvanceVirtualTime(virtual_time_when_paused_ +
                                        base::TimeDelta::FromMilliseconds(10));
  }
  TRACE_EVENT_NESTABLE_ASYNC_END0(
      "renderer.scheduler", "WebScopedVirtualTimePauser::PauseVirtualTime",
      trace_id_);
}

}  // namespace blink
