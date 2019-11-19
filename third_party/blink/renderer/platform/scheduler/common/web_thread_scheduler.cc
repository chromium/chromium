// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"

#include <utility>

#include "base/feature_list.h"
#include "base/message_loop/message_pump_type.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/scheduler/common/features.h"
#include "third_party/blink/renderer/platform/scheduler/common/tracing_helper.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"

namespace blink {
namespace scheduler {

WebThreadScheduler::~WebThreadScheduler() = default;

// static
std::unique_ptr<WebThreadScheduler>
WebThreadScheduler::CreateMainThreadScheduler(
    std::unique_ptr<base::MessagePump> message_pump,
    base::Optional<base::Time> initial_virtual_time) {
  auto settings =
      base::sequence_manager::SequenceManager::Settings::Builder()
          .SetMessagePumpType(base::MessagePumpType::DEFAULT)
          .SetRandomisedSamplingEnabled(true)
          .SetAddQueueTimeToTasks(true)
          .SetAntiStarvationLogicForPrioritiesDisabled(
              base::FeatureList::IsEnabled(
                  kBlinkSchedulerDisableAntiStarvationForPriorities))
          .Build();
  auto sequence_manager =
      message_pump
          ? base::sequence_manager::
                CreateSequenceManagerOnCurrentThreadWithPump(
                    std::move(message_pump), std::move(settings))
          : base::sequence_manager::CreateSequenceManagerOnCurrentThread(
                std::move(settings));
  std::unique_ptr<MainThreadSchedulerImpl> scheduler(
      new MainThreadSchedulerImpl(std::move(sequence_manager),
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

scoped_refptr<base::SingleThreadTaskRunner>
WebThreadScheduler::DeprecatedDefaultTaskRunner() {
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

void WebThreadScheduler::WillPostInputEventToMainThread(
    WebInputEvent::Type web_input_event_type) {
  NOTREACHED();
}

void WebThreadScheduler::WillHandleInputEventOnMainThread(
    WebInputEvent::Type web_input_event_type) {
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

void WebThreadScheduler::DidScheduleBeginMainFrame() {
  NOTREACHED();
}

void WebThreadScheduler::DidRunBeginMainFrame() {
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

void WebThreadScheduler::SetRendererProcessType(WebRendererProcessType type) {
  NOTREACHED();
}

void WebThreadScheduler::OnMainFrameRequestedForInput() {
  NOTREACHED();
}

}  // namespace scheduler
}  // namespace blink
