// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_SCHEDULER_TEST_WEB_MOCK_THREAD_SCHEDULER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_SCHEDULER_TEST_WEB_MOCK_THREAD_SCHEDULER_H_

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"

namespace base {
class TaskObserver;
}

namespace blink {
namespace scheduler {

class WebMockThreadScheduler : public WebThreadScheduler {
 public:
  WebMockThreadScheduler() = default;
  ~WebMockThreadScheduler() override = default;

  MOCK_METHOD0(DefaultTaskRunner,
               scoped_refptr<base::SingleThreadTaskRunner>());
  MOCK_METHOD0(CompositorTaskRunner,
               scoped_refptr<base::SingleThreadTaskRunner>());
  MOCK_METHOD0(InputTaskRunner, scoped_refptr<base::SingleThreadTaskRunner>());
  MOCK_METHOD0(LoadingTaskRunner,
               scoped_refptr<base::SingleThreadTaskRunner>());
  MOCK_METHOD0(IPCTaskRunner, scoped_refptr<base::SingleThreadTaskRunner>());
  MOCK_METHOD0(NewRenderWidgetSchedulingState,
               std::unique_ptr<WebRenderWidgetSchedulingState>());
  MOCK_METHOD1(WillBeginFrame, void(const viz::BeginFrameArgs&));
  MOCK_METHOD0(BeginFrameNotExpectedSoon, void());
  MOCK_METHOD1(BeginMainFrameNotExpectedUntil, void(base::TimeTicks));
  MOCK_METHOD0(DidCommitFrameToCompositor, void());
  MOCK_METHOD2(DidHandleInputEventOnCompositorThread,
               void(const WebInputEvent&, InputEventState));
  MOCK_METHOD1(WillPostInputEventToMainThread, void(WebInputEvent::Type));
  MOCK_METHOD1(WillHandleInputEventOnMainThread, void(WebInputEvent::Type));
  MOCK_METHOD2(DidHandleInputEventOnMainThread,
               void(const WebInputEvent&, WebInputEventResult));
  MOCK_METHOD0(DidAnimateForInputOnCompositorThread, void());
  MOCK_METHOD0(DidScheduleBeginMainFrame, void());
  MOCK_METHOD0(DidRunBeginMainFrame, void());
  MOCK_METHOD1(SetRendererHidden, void(bool));
  MOCK_METHOD1(SetRendererBackgrounded, void(bool));
  MOCK_METHOD1(SetSchedulerKeepActive, void(bool));
  MOCK_METHOD0(PauseRenderer, std::unique_ptr<RendererPauseHandle>());
#if defined(OS_ANDROID)
  MOCK_METHOD0(PauseTimersForAndroidWebView, void());
  MOCK_METHOD0(ResumeTimersForAndroidWebView, void());
#endif
  MOCK_METHOD0(OnNavigate, void());
  MOCK_METHOD0(IsHighPriorityWorkAnticipated, bool());
  MOCK_METHOD1(AddTaskObserver, void(base::TaskObserver*));
  MOCK_METHOD1(RemoveTaskObserver, void(base::TaskObserver*));
  MOCK_METHOD0(Shutdown, void());
  MOCK_METHOD0(VirtualTimePaused, void());
  MOCK_METHOD0(VirtualTimeResumed, void());
  MOCK_METHOD1(SetTopLevelBlameContext, void(base::trace_event::BlameContext*));
  MOCK_METHOD1(SetRendererProcessType, void(WebRendererProcessType));
  MOCK_METHOD0(OnMainFrameRequestedForInput, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(WebMockThreadScheduler);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_SCHEDULER_TEST_WEB_MOCK_THREAD_SCHEDULER_H_
