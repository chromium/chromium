// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_SCHEDULER_TEST_WEB_FAKE_THREAD_SCHEDULER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_SCHEDULER_TEST_WEB_FAKE_THREAD_SCHEDULER_H_

#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"

namespace blink {
namespace scheduler {

class WebFakeThreadScheduler : public WebThreadScheduler {
 public:
  WebFakeThreadScheduler();
  WebFakeThreadScheduler(const WebFakeThreadScheduler&) = delete;
  WebFakeThreadScheduler& operator=(const WebFakeThreadScheduler&) = delete;
  ~WebFakeThreadScheduler() override;

  // RendererScheduler implementation.
  std::unique_ptr<Thread> CreateMainThread() override;
  scoped_refptr<base::SingleThreadTaskRunner> DefaultTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> CompositorTaskRunner() override;
  std::unique_ptr<WebAgentGroupScheduler> CreateAgentGroupScheduler() override;
  std::unique_ptr<WebWidgetScheduler> CreateWidgetScheduler() override;
  WebAgentGroupScheduler* GetCurrentAgentGroupScheduler() override;
  std::unique_ptr<WebRenderWidgetSchedulingState>
  NewRenderWidgetSchedulingState() override;
  void WillBeginFrame(const viz::BeginFrameArgs& args) override;
  void BeginFrameNotExpectedSoon() override;
  void BeginMainFrameNotExpectedUntil(base::TimeTicks time) override;
  void DidCommitFrameToCompositor() override;
  void DidHandleInputEventOnCompositorThread(
      const WebInputEvent& web_input_event,
      InputEventState event_state) override;
  void WillPostInputEventToMainThread(
      WebInputEvent::Type web_input_event_type,
      const WebInputEventAttribution& attribution) override;
  void WillHandleInputEventOnMainThread(
      WebInputEvent::Type web_input_event_type,
      const WebInputEventAttribution& attribution) override;
  void DidHandleInputEventOnMainThread(const WebInputEvent& web_input_event,
                                       WebInputEventResult result) override;
  void DidAnimateForInputOnCompositorThread() override;
  void DidScheduleBeginMainFrame() override;
  void DidRunBeginMainFrame() override;
  void SetRendererHidden(bool hidden) override;
  void SetRendererBackgrounded(bool backgrounded) override;
  std::unique_ptr<RendererPauseHandle> PauseRenderer() override;
#if defined(OS_ANDROID)
  void PauseTimersForAndroidWebView() override;
  void ResumeTimersForAndroidWebView() override;
#endif
  bool IsHighPriorityWorkAnticipated() override;
  void Shutdown() override;
  void SetTopLevelBlameContext(
      base::trace_event::BlameContext* blame_context) override;
  void SetRendererProcessType(WebRendererProcessType type) override;
  void OnMainFrameRequestedForInput() override;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_SCHEDULER_TEST_WEB_FAKE_THREAD_SCHEDULER_H_
