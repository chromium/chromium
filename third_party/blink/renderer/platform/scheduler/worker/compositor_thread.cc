// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/worker/compositor_thread.h"

#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/hang_watcher.h"
#include "base/threading/platform_thread_metrics.h"
#include "mojo/public/cpp/bindings/interface_endpoint_client.h"
#include "third_party/blink/renderer/platform/scheduler/worker/compositor_thread_scheduler_impl.h"

namespace blink {
namespace scheduler {

CompositorThread::CompositorThread(const ThreadCreationParams& params)
    : NonMainThreadImpl(params) {}

CompositorThread::~CompositorThread() {
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());
}

void CompositorThread::InitializeHangWatcherAndThreadName() {
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());

  if (base::HangWatcher::IsCompositorThreadHangWatchingEnabled()) {
    hang_watcher_registration_ = base::HangWatcher::RegisterThread(
        base::HangWatcher::ThreadType::kCompositorThread);
  }

  mojo::InterfaceEndpointClient::SetThreadNameSuffixForMetrics("Compositor");
#if BUILDFLAG(IS_ANDROID)
  base::PlatformThreadPriorityMonitor::Get().RegisterCurrentThread(
      "Compositor");
#endif  // BUILDFLAG(IS_ANDROID)
}

std::unique_ptr<NonMainThreadSchedulerBase>
CompositorThread::CreateNonMainThreadScheduler(
    base::sequence_manager::SequenceManager* sequence_manager) {
  return std::make_unique<CompositorThreadSchedulerImpl>(sequence_manager);
}

}  // namespace scheduler
}  // namespace blink
