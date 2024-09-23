// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/scheduler/test/web_fake_thread_scheduler.h"

#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/input/web_input_event_attribution.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_agent_group_scheduler_scheduler.h"

namespace blink {
namespace scheduler {

WebFakeThreadScheduler::WebFakeThreadScheduler() = default;

WebFakeThreadScheduler::~WebFakeThreadScheduler() = default;

std::unique_ptr<MainThread> WebFakeThreadScheduler::CreateMainThread() {
  return nullptr;
}

std::unique_ptr<WebAgentGroupScheduler>
WebFakeThreadScheduler::CreateWebAgentGroupScheduler() {
  return std::make_unique<WebAgentGroupScheduler>(
      MakeGarbageCollected<FakeAgentGroupScheduler>(*this));
}

void WebFakeThreadScheduler::SetRendererHidden(bool hidden) {}

void WebFakeThreadScheduler::SetRendererBackgrounded(bool backgrounded) {}

#if BUILDFLAG(IS_ANDROID)
void WebFakeThreadScheduler::PauseTimersForAndroidWebView() {}

void WebFakeThreadScheduler::ResumeTimersForAndroidWebView() {}
#endif

void WebFakeThreadScheduler::Shutdown() {}

void WebFakeThreadScheduler::SetRendererProcessType(
    WebRendererProcessType type) {}

}  // namespace scheduler
}  // namespace blink
