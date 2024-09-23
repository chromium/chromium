// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/widget_scheduler_impl.h"

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"

namespace blink::scheduler {

WidgetSchedulerImpl::WidgetSchedulerImpl(
    MainThreadSchedulerImpl* main_thread_scheduler,
    RenderWidgetSignals* render_widget_signals)
    : main_thread_scheduler_(main_thread_scheduler),
      render_widget_signals_(render_widget_signals) {
  DCHECK(render_widget_signals_);

  // main_thread_scheduler_ may be null in some tests.
  if (main_thread_scheduler_) {
    input_task_queue_ = main_thread_scheduler->NewTaskQueue(
        MainThreadTaskQueue::QueueCreationParams(
            MainThreadTaskQueue::QueueType::kInput)
            .SetShouldMonitorQuiescence(true)
            .SetPrioritisationType(
                MainThreadTaskQueue::QueueTraits::PrioritisationType::kInput));
    input_task_runner_ = input_task_queue_->CreateTaskRunner(
        TaskType::kMainThreadTaskQueueInput);
    input_task_queue_enabled_voter_ =
        input_task_queue_->CreateQueueEnabledVoter();
  }

  render_widget_signals_->IncNumVisibleRenderWidgets();
}

WidgetSchedulerImpl::~WidgetSchedulerImpl() = default;

void WidgetSchedulerImpl::Shutdown() {
  if (input_task_queue_) {
    input_task_queue_enabled_voter_.reset();
    input_task_runner_.reset();
    input_task_queue_->ShutdownTaskQueue();
    input_task_queue_.reset();
  }

  if (!hidden_) {
    render_widget_signals_->DecNumVisibleRenderWidgets();
  }
}

scoped_refptr<base::SingleThreadTaskRunner>
WidgetSchedulerImpl::InputTaskRunner() {
  return input_task_runner_;
}

void WidgetSchedulerImpl::WillBeginFrame(const viz::BeginFrameArgs& args) {
  main_thread_scheduler_->WillBeginFrame(args);
}

void WidgetSchedulerImpl::BeginFrameNotExpectedSoon() {
  main_thread_scheduler_->BeginFrameNotExpectedSoon();
}

void WidgetSchedulerImpl::BeginMainFrameNotExpectedUntil(base::TimeTicks time) {
  main_thread_scheduler_->BeginMainFrameNotExpectedUntil(time);
}

void WidgetSchedulerImpl::DidCommitFrameToCompositor() {
  main_thread_scheduler_->DidCommitFrameToCompositor();
}

void WidgetSchedulerImpl::DidHandleInputEventOnCompositorThread(
    const WebInputEvent& web_input_event,
    InputEventState event_state) {
  main_thread_scheduler_->DidHandleInputEventOnCompositorThread(web_input_event,
                                                                event_state);
}

void WidgetSchedulerImpl::WillPostInputEventToMainThread(
    WebInputEvent::Type web_input_event_type,
    const WebInputEventAttribution& web_input_event_attribution) {
  main_thread_scheduler_->WillPostInputEventToMainThread(
      web_input_event_type, web_input_event_attribution);
}

void WidgetSchedulerImpl::WillHandleInputEventOnMainThread(
    WebInputEvent::Type web_input_event_type,
    const WebInputEventAttribution& web_input_event_attribution) {
  main_thread_scheduler_->WillHandleInputEventOnMainThread(
      web_input_event_type, web_input_event_attribution);
}

void WidgetSchedulerImpl::DidHandleInputEventOnMainThread(
    const WebInputEvent& web_input_event,
    WebInputEventResult result,
    bool frame_requested) {
  main_thread_scheduler_->DidHandleInputEventOnMainThread(
      web_input_event, result, frame_requested);
}

void WidgetSchedulerImpl::DidRunBeginMainFrame() {}

void WidgetSchedulerImpl::SetHidden(bool hidden) {
  if (hidden_ == hidden)
    return;

  hidden_ = hidden;

  if (hidden_) {
    render_widget_signals_->DecNumVisibleRenderWidgets();
  } else {
    render_widget_signals_->IncNumVisibleRenderWidgets();
  }
}

}  // namespace blink::scheduler
