// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"

#include <utility>

#include "base/feature_list.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/input/web_input_event_attribution.h"
#include "third_party/blink/renderer/platform/scheduler/common/features.h"
#include "third_party/blink/renderer/platform/scheduler/common/task_priority.h"
#include "third_party/blink/renderer/platform/scheduler/common/tracing_helper.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"

namespace blink {
namespace scheduler {

WebThreadScheduler::~WebThreadScheduler() = default;

// static
std::unique_ptr<WebThreadScheduler>
WebThreadScheduler::CreateMainThreadScheduler(
    std::unique_ptr<base::MessagePump> message_pump) {
  auto settings = base::sequence_manager::SequenceManager::Settings::Builder()
                      .SetMessagePumpType(base::MessagePumpType::DEFAULT)
                      .SetRandomisedSamplingEnabled(true)
                      .SetAddQueueTimeToTasks(true)
                      .SetPrioritySettings(CreatePrioritySettings())
                      .Build();
  auto sequence_manager =
      message_pump
          ? base::sequence_manager::
                CreateSequenceManagerOnCurrentThreadWithPump(
                    std::move(message_pump), std::move(settings))
          : base::sequence_manager::CreateSequenceManagerOnCurrentThread(
                std::move(settings));
  return std::make_unique<MainThreadSchedulerImpl>(std::move(sequence_manager));
}

// Stubs for main thread only virtual functions.
scoped_refptr<base::SingleThreadTaskRunner>
WebThreadScheduler::DeprecatedDefaultTaskRunner() {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

std::unique_ptr<MainThread> WebThreadScheduler::CreateMainThread() {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

void WebThreadScheduler::SetRendererHidden(bool hidden) {
  NOTREACHED_IN_MIGRATION();
}

void WebThreadScheduler::SetRendererBackgrounded(bool backgrounded) {
  NOTREACHED_IN_MIGRATION();
}

#if BUILDFLAG(IS_ANDROID)
void WebThreadScheduler::PauseTimersForAndroidWebView() {
  NOTREACHED_IN_MIGRATION();
}

void WebThreadScheduler::ResumeTimersForAndroidWebView() {
  NOTREACHED_IN_MIGRATION();
}
#endif  // BUILDFLAG(IS_ANDROID)

void WebThreadScheduler::SetRendererProcessType(WebRendererProcessType type) {
  NOTREACHED_IN_MIGRATION();
}

void WebThreadScheduler::OnUrgentMessageReceived() {
  NOTREACHED_IN_MIGRATION();
}

void WebThreadScheduler::OnUrgentMessageProcessed() {
  NOTREACHED_IN_MIGRATION();
}

}  // namespace scheduler
}  // namespace blink
