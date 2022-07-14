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
  scoped_refptr<base::SingleThreadTaskRunner> CompositorTaskRunner() override;
  std::unique_ptr<WebAgentGroupScheduler> CreateAgentGroupScheduler() override;
  WebAgentGroupScheduler* GetCurrentAgentGroupScheduler() override;
  void SetRendererHidden(bool hidden) override;
  void SetRendererBackgrounded(bool backgrounded) override;
  std::unique_ptr<RendererPauseHandle> PauseRenderer() override;
#if BUILDFLAG(IS_ANDROID)
  void PauseTimersForAndroidWebView() override;
  void ResumeTimersForAndroidWebView() override;
#endif
  void Shutdown() override;
  void SetTopLevelBlameContext(
      base::trace_event::BlameContext* blame_context) override;
  void SetRendererProcessType(WebRendererProcessType type) override;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_SCHEDULER_TEST_WEB_FAKE_THREAD_SCHEDULER_H_
