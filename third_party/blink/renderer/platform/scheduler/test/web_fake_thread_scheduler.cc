// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/scheduler/test/web_fake_thread_scheduler.h"

#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"

namespace blink {
namespace scheduler {

WebFakeThreadScheduler::WebFakeThreadScheduler() = default;

WebFakeThreadScheduler::~WebFakeThreadScheduler() = default;

std::unique_ptr<Thread> WebFakeThreadScheduler::CreateMainThread() {
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
WebFakeThreadScheduler::DefaultTaskRunner() {
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
WebFakeThreadScheduler::CompositorTaskRunner() {
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
WebFakeThreadScheduler::InputTaskRunner() {
  return base::ThreadTaskRunnerHandle::Get();
}

scoped_refptr<base::SingleThreadTaskRunner>
WebFakeThreadScheduler::IPCTaskRunner() {
  return nullptr;
}

std::unique_ptr<WebRenderWidgetSchedulingState>
WebFakeThreadScheduler::NewRenderWidgetSchedulingState() {
  return nullptr;
}

void WebFakeThreadScheduler::WillBeginFrame(const viz::BeginFrameArgs& args) {}

void WebFakeThreadScheduler::BeginFrameNotExpectedSoon() {}

void WebFakeThreadScheduler::BeginMainFrameNotExpectedUntil(
    base::TimeTicks time) {}

void WebFakeThreadScheduler::DidCommitFrameToCompositor() {}

void WebFakeThreadScheduler::DidHandleInputEventOnCompositorThread(
    const blink::WebInputEvent& web_input_event,
    InputEventState event_state) {}

void WebFakeThreadScheduler::WillPostInputEventToMainThread(
    WebInputEvent::Type web_input_event_type) {}

void WebFakeThreadScheduler::WillHandleInputEventOnMainThread(
    WebInputEvent::Type web_input_event_type) {}

void WebFakeThreadScheduler::DidHandleInputEventOnMainThread(
    const blink::WebInputEvent& web_input_event,
    WebInputEventResult result) {}

void WebFakeThreadScheduler::DidAnimateForInputOnCompositorThread() {}

void WebFakeThreadScheduler::DidScheduleBeginMainFrame() {}
void WebFakeThreadScheduler::DidRunBeginMainFrame() {}

bool WebFakeThreadScheduler::IsHighPriorityWorkAnticipated() {
  return false;
}

void WebFakeThreadScheduler::SetRendererHidden(bool hidden) {}

void WebFakeThreadScheduler::SetRendererBackgrounded(bool backgrounded) {}

void WebFakeThreadScheduler::SetSchedulerKeepActive(bool keep_active) {}

std::unique_ptr<WebFakeThreadScheduler::RendererPauseHandle>
WebFakeThreadScheduler::PauseRenderer() {
  return nullptr;
}

#if defined(OS_ANDROID)
void WebFakeThreadScheduler::PauseTimersForAndroidWebView() {}

void WebFakeThreadScheduler::ResumeTimersForAndroidWebView() {}
#endif

void WebFakeThreadScheduler::Shutdown() {}

void WebFakeThreadScheduler::SetTopLevelBlameContext(
    base::trace_event::BlameContext* blame_context) {}

void WebFakeThreadScheduler::SetRendererProcessType(
    WebRendererProcessType type) {}

void WebFakeThreadScheduler::OnMainFrameRequestedForInput() {}

}  // namespace scheduler
}  // namespace blink
