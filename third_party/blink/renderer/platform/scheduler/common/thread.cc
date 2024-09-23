// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/thread.h"

#include "base/feature_list.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/interface_endpoint_client.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/worker/compositor_thread.h"
#include "third_party/blink/renderer/platform/scheduler/worker/compositor_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include <unistd.h>
#endif

namespace blink {

namespace {

constinit thread_local Thread* current_thread = nullptr;

std::unique_ptr<MainThread>& GetMainThread() {
  DEFINE_STATIC_LOCAL(std::unique_ptr<MainThread>, main_thread, ());
  return main_thread;
}

std::unique_ptr<NonMainThread>& GetCompositorThread() {
  DEFINE_STATIC_LOCAL(std::unique_ptr<NonMainThread>, compositor_thread, ());
  return compositor_thread;
}

}  // namespace

// static
void Thread::UpdateThreadTLS(Thread* thread) {
  current_thread = thread;
}

ThreadCreationParams::ThreadCreationParams(ThreadType thread_type)
    : thread_type(thread_type),
      name(GetNameForThreadType(thread_type)),
      frame_or_worker_scheduler(nullptr),
      supports_gc(false) {}

ThreadCreationParams& ThreadCreationParams::SetThreadNameForTest(
    const char* thread_name) {
  name = thread_name;
  return *this;
}

ThreadCreationParams& ThreadCreationParams::SetFrameOrWorkerScheduler(
    FrameOrWorkerScheduler* scheduler) {
  frame_or_worker_scheduler = scheduler;
  return *this;
}

ThreadCreationParams& ThreadCreationParams::SetSupportsGC(bool gc_enabled) {
  supports_gc = gc_enabled;
  return *this;
}

void Thread::CreateAndSetCompositorThread() {
  DCHECK(!GetCompositorThread());

  ThreadCreationParams params(ThreadType::kCompositorThread);
  params.base_thread_type = base::ThreadType::kDisplayCritical;

  auto compositor_thread =
      std::make_unique<scheduler::CompositorThread>(params);
  compositor_thread->Init();
  compositor_thread->GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce([]() {
        mojo::InterfaceEndpointClient::SetThreadNameSuffixForMetrics(
            "Compositor");
      }));

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  compositor_thread->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&base::PlatformThread::CurrentId),
      base::BindOnce([](base::PlatformThreadId compositor_thread_id) {
        // Chrome OS moves tasks between control groups on thread priority
        // changes. This is not possible inside the sandbox, so ask the
        // browser to do it.
        Platform::Current()->SetThreadType(compositor_thread_id,
                                           base::ThreadType::kDisplayCritical);
      }));
#endif

  GetCompositorThread() = std::move(compositor_thread);
}

Thread* Thread::Current() {
  return current_thread;
}

MainThread* Thread::MainThread() {
  return GetMainThread().get();
}

NonMainThread* Thread::CompositorThread() {
  return GetCompositorThread().get();
}

std::unique_ptr<MainThread> MainThread::SetMainThread(
    std::unique_ptr<MainThread> main_thread) {
  current_thread = main_thread.get();
  std::swap(GetMainThread(), main_thread);
  return main_thread;
}

Thread::Thread() = default;

Thread::~Thread() = default;

bool Thread::IsCurrentThread() const {
  return current_thread == this;
}

void Thread::AddTaskObserver(TaskObserver* task_observer) {
  CHECK(IsCurrentThread());
  Scheduler()->AddTaskObserver(task_observer);
}

void Thread::RemoveTaskObserver(TaskObserver* task_observer) {
  CHECK(IsCurrentThread());
  Scheduler()->RemoveTaskObserver(task_observer);
}

#if BUILDFLAG(IS_WIN)
static_assert(sizeof(blink::PlatformThreadId) >= sizeof(DWORD),
              "size of platform thread id is too small");
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
static_assert(sizeof(blink::PlatformThreadId) >= sizeof(pid_t),
              "size of platform thread id is too small");
#else
#error Unexpected platform
#endif

}  // namespace blink
