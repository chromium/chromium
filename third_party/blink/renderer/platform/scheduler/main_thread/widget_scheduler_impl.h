// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_WIDGET_SCHEDULER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_WIDGET_SCHEDULER_IMPL_H_

#include <memory>

#include "base/task/sequence_manager/task_queue.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/public/widget_scheduler.h"

namespace blink::scheduler {

class MainThreadSchedulerImpl;
class MainThreadTaskQueue;
class RenderWidgetSignals;

class PLATFORM_EXPORT WidgetSchedulerImpl : public WidgetScheduler {
 public:
  WidgetSchedulerImpl(MainThreadSchedulerImpl*, RenderWidgetSignals*);
  WidgetSchedulerImpl(const WidgetSchedulerImpl&) = delete;
  WidgetSchedulerImpl& operator=(const WidgetSchedulerImpl&) = delete;
  ~WidgetSchedulerImpl() override;

  // WidgetScheduler overrides:
  void Shutdown() override;
  scoped_refptr<base::SingleThreadTaskRunner> InputTaskRunner() override;
  void WillBeginFrame(const viz::BeginFrameArgs& args) override;
  void BeginFrameNotExpectedSoon() override;
  void BeginMainFrameNotExpectedUntil(base::TimeTicks time) override;
  void DidCommitFrameToCompositor() override;
  void DidHandleInputEventOnCompositorThread(
      const WebInputEvent& web_input_event,
      InputEventState event_state) override;
  void WillPostInputEventToMainThread(
      WebInputEvent::Type web_input_event_type,
      const WebInputEventAttribution& web_input_event_attribution) override;
  void WillHandleInputEventOnMainThread(
      WebInputEvent::Type web_input_event_type,
      const WebInputEventAttribution& web_input_event_attribution) override;
  void DidHandleInputEventOnMainThread(const WebInputEvent& web_input_event,
                                       WebInputEventResult result) override;
  void DidAnimateForInputOnCompositorThread() override;
  void DidRunBeginMainFrame() override;
  void SetHidden(bool hidden) override;
  void SetHasTouchHandler(bool has_touch_handler) override;

 private:
  scoped_refptr<MainThreadTaskQueue> input_task_queue_;
  scoped_refptr<base::SingleThreadTaskRunner> input_task_runner_;
  std::unique_ptr<base::sequence_manager::TaskQueue::QueueEnabledVoter>
      input_task_queue_enabled_voter_;

  MainThreadSchedulerImpl* const main_thread_scheduler_;
  RenderWidgetSignals* const render_widget_signals_;
  bool hidden_ = false;
  bool has_touch_handler_ = false;
};

}  // namespace blink::scheduler

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_WIDGET_SCHEDULER_IMPL_H_
