// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/widget_scheduler_impl.h"

#include "base/feature_list.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"

namespace blink::scheduler {

namespace {
BASE_FEATURE(kDeferTasksAfterInputOnlyWhenRenderingUnpaused,
             base::FEATURE_ENABLED_BY_DEFAULT);
}  // namespace

WidgetSchedulerImpl::WidgetSchedulerImpl(
    MainThreadSchedulerImpl* main_thread_scheduler,
    RenderWidgetSignals* render_widget_signals,
    Delegate* delegate)
    : main_thread_scheduler_(main_thread_scheduler),
      render_widget_signals_(render_widget_signals),
      delegate_(delegate) {
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

void WidgetSchedulerImpl::WillShutdown() {
  if (main_thread_scheduler_) {
    main_thread_scheduler_->OnWidgetSchedulerWillShutdown(this);
  }
  delegate_ = nullptr;
}

void WidgetSchedulerImpl::Shutdown() {
  // Check that `WillShutdown()` was called first.
  CHECK(!delegate_);

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
  // We always assume there will be another frame until notified. Note that we
  // might not be receiving compositor signals here, so we might not know if
  // this will remain these case.
  begin_frame_not_expected_soon_ = false;
  main_thread_scheduler_->WillBeginFrame(args);
}

void WidgetSchedulerImpl::BeginFrameNotExpectedSoon() {
  begin_frame_not_expected_soon_ = true;
  main_thread_scheduler_->BeginFrameNotExpectedSoon();
}

void WidgetSchedulerImpl::BeginMainFrameNotExpectedUntil(base::TimeTicks time) {
  begin_frame_not_expected_soon_ = false;
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
  // TODO(crbug.com/451389811): We should probably move `frame_requested` to the
  // `Delegate`.
  bool is_frame_expected = frame_requested;
  if (base::FeatureList::IsEnabled(
          kDeferTasksAfterInputOnlyWhenRenderingUnpaused)) {
    // `delegate_` can be null in unit tests, or if input tasks run during
    // shutdown between `WillShutdown()` and actually destroying the widget
    // scheduler, which also happens in tests.
    if (delegate_) {
      // A frame is not expected if rendering is deferred, which happens early
      // in page load, or paused, which happens during view transitions. We do,
      // however, expect a main frame with all the requisite callbacks if frame
      // commits are deferred.
      is_frame_expected &= !delegate_->AreMainFramesPausedOrDeferred();
    }
  }
  main_thread_scheduler_->DidHandleInputEventOnMainThread(
      web_input_event, result, is_frame_expected);
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

void WidgetSchedulerImpl::RequestBeginMainFrameNotExpected(bool requested) {
  // Either `BeginFrameNotExpectedSoon()` will be sent soon if applicable, or
  // we're no longer tracking if this is the case. In either case, we don't know
  // if this is true, so assume not.
  begin_frame_not_expected_soon_ = false;
  CHECK(delegate_);
  delegate_->RequestBeginMainFrameNotExpected(requested);
}

}  // namespace blink::scheduler
