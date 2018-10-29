// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"

#include <utility>
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/scheduler/common/tracing_helper.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"

namespace blink {
namespace scheduler {

WebThreadScheduler::~WebThreadScheduler() = default;

// static
std::unique_ptr<WebThreadScheduler>
WebThreadScheduler::CreateMainThreadScheduler(
    base::Optional<base::Time> initial_virtual_time) {
  // Ensure categories appear as an option in chrome://tracing.
  WarmupTracingCategories();
  // Workers might be short-lived, so placing warmup here.
  TRACE_EVENT_WARMUP_CATEGORY(TRACE_DISABLED_BY_DEFAULT("worker.scheduler"));

  std::unique_ptr<MainThreadSchedulerImpl> scheduler(
      new MainThreadSchedulerImpl(
          base::sequence_manager::CreateSequenceManagerOnCurrentThread(),
          initial_virtual_time));
  return std::move(scheduler);
}

// static
const char* WebThreadScheduler::InputEventStateToString(
    InputEventState input_event_state) {
  switch (input_event_state) {
    case InputEventState::EVENT_CONSUMED_BY_COMPOSITOR:
      return "event_consumed_by_compositor";
    case InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD:
      return "event_forwarded_to_main_thread";
    default:
      NOTREACHED();
      return nullptr;
  }
}

// Stubs for main thread only virtual functions.
scoped_refptr<base::SingleThreadTaskRunner>
WebThreadScheduler::DefaultTaskRunner() {
  NOTREACHED();
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
WebThreadScheduler::CompositorTaskRunner() {
  NOTREACHED();
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
WebThreadScheduler::InputTaskRunner() {
  NOTREACHED();
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
WebThreadScheduler::IPCTaskRunner() {
  NOTREACHED();
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
WebThreadScheduler::CleanupTaskRunner() {
  NOTREACHED();
  return nullptr;
}

std::unique_ptr<Thread> WebThreadScheduler::CreateMainThread() {
  NOTREACHED();
  return nullptr;
}

std::unique_ptr<WebRenderWidgetSchedulingState>
WebThreadScheduler::NewRenderWidgetSchedulingState() {
  NOTREACHED();
  return nullptr;
}

void WebThreadScheduler::BeginFrameNotExpectedSoon() {
  NOTREACHED();
}

void WebThreadScheduler::BeginMainFrameNotExpectedUntil(base::TimeTicks time) {
  NOTREACHED();
}

void WebThreadScheduler::WillBeginFrame(const viz::BeginFrameArgs& args) {
  NOTREACHED();
}

void WebThreadScheduler::DidCommitFrameToCompositor() {
  NOTREACHED();
}

void WebThreadScheduler::DidHandleInputEventOnCompositorThread(
    const WebInputEvent& web_input_event,
    InputEventState event_state) {
  NOTREACHED();
}

void WebThreadScheduler::DidHandleInputEventOnMainThread(
    const WebInputEvent& web_input_event,
    WebInputEventResult result) {
  NOTREACHED();
}

void WebThreadScheduler::DidAnimateForInputOnCompositorThread() {
  NOTREACHED();
}

void WebThreadScheduler::SetRendererHidden(bool hidden) {
  NOTREACHED();
}

void WebThreadScheduler::SetRendererBackgrounded(bool backgrounded) {
  NOTREACHED();
}

void WebThreadScheduler::SetSchedulerKeepActive(bool keep_active) {
  NOTREACHED();
}

#if defined(OS_ANDROID)
void WebThreadScheduler::PauseTimersForAndroidWebView() {
  NOTREACHED();
}

void WebThreadScheduler::ResumeTimersForAndroidWebView() {
  NOTREACHED();
}
#endif  // defined(OS_ANDROID)

std::unique_ptr<WebThreadScheduler::RendererPauseHandle>
WebThreadScheduler::PauseRenderer() {
  NOTREACHED();
  return nullptr;
}

bool WebThreadScheduler::IsHighPriorityWorkAnticipated() {
  NOTREACHED();
  return false;
}

void WebThreadScheduler::SetTopLevelBlameContext(
    base::trace_event::BlameContext* blame_context) {
  NOTREACHED();
}

void WebThreadScheduler::AddRAILModeObserver(WebRAILModeObserver* observer) {
  NOTREACHED();
}

void WebThreadScheduler::SetRendererProcessType(RendererProcessType type) {
  NOTREACHED();
}

WebScopedVirtualTimePauser WebThreadScheduler::CreateWebScopedVirtualTimePauser(
    const char* name,
    WebScopedVirtualTimePauser::VirtualTaskDuration duration) {
  NOTREACHED();
  return WebScopedVirtualTimePauser();
}

void WebThreadScheduler::OnMainFrameRequestedForInput() {
  NOTREACHED();
}

}  // namespace scheduler
}  // namespace blink
