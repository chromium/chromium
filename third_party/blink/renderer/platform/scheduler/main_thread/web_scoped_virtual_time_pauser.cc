// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/scheduler/web_scoped_virtual_time_pauser.h"

#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/blink/renderer/platform/scheduler/common/thread_scheduler_base.h"

namespace blink {

WebScopedVirtualTimePauser::WebScopedVirtualTimePauser()
    : scheduler_(nullptr) {}

WebScopedVirtualTimePauser::WebScopedVirtualTimePauser(
    scheduler::ThreadSchedulerBase* scheduler,
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
  // Note that virtual time can never be disabled after it's enabled once, so we
  // don't need to worry about the reverse transition.
  virtual_time_enabled_when_paused_ = scheduler_->IsVirtualTimeEnabled();

  if (virtual_time_enabled_when_paused_) {
    // This trace event shows when individual pausers are active (instead of the
    // global paused/unpaused state).
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
        "renderer.scheduler", "WebScopedVirtualTimePauser::PauseVirtualTime",
        trace_id_, "name", debug_name_.Latin1());
  }
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
                                        base::Milliseconds(10));
  }
  if (virtual_time_enabled_when_paused_) {
    TRACE_EVENT_NESTABLE_ASYNC_END0(
        "renderer.scheduler", "WebScopedVirtualTimePauser::PauseVirtualTime",
        trace_id_);
  }
}

}  // namespace blink
