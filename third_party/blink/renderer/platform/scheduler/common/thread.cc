// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/thread.h"

#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "build/build_config.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/scheduler/worker/compositor_thread.h"
#include "third_party/blink/renderer/platform/scheduler/worker/compositor_thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_thread.h"
#include "third_party/blink/renderer/platform/web_task_runner.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
#include <unistd.h>
#endif

namespace blink {

namespace {

// Thread-local storage for "blink::Thread"s.
Thread*& ThreadTLSSlot() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(WTF::ThreadSpecific<Thread*>, thread_tls_slot,
                                  ());
  return *thread_tls_slot;
}

// Update the threads TLS on the newly created thread.
void UpdateThreadTLS(Thread* thread, base::WaitableEvent* event) {
  ThreadTLSSlot() = thread;
  event->Signal();
}

// Post a task to register |thread| to the TLS, and wait until it gets actually
// registered. This is called on a thread that created |thread| (not on
// |thread|.)
void UpdateThreadTLSAndWait(Thread* thread) {
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  PostCrossThreadTask(
      *thread->GetTaskRunner(), FROM_HERE,
      CrossThreadBind(&UpdateThreadTLS, WTF::CrossThreadUnretained(thread),
                      WTF::CrossThreadUnretained(&event)));
  event.Wait();
}

std::unique_ptr<Thread>& GetMainThread() {
  DEFINE_STATIC_LOCAL(std::unique_ptr<Thread>, main_thread, ());
  return main_thread;
}

std::unique_ptr<Thread>& GetCompositorThread() {
  DEFINE_STATIC_LOCAL(std::unique_ptr<Thread>, compositor_thread, ());
  return compositor_thread;
}

}  // namespace

ThreadCreationParams::ThreadCreationParams(WebThreadType thread_type)
    : thread_type(thread_type),
      name(GetNameForThreadType(thread_type)),
      frame_or_worker_scheduler(nullptr) {}

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

std::unique_ptr<Thread> Thread::CreateThread(
    const ThreadCreationParams& params) {
  auto thread = std::make_unique<scheduler::WorkerThread>(params);
  thread->Init();
  UpdateThreadTLSAndWait(thread.get());
  return std::move(thread);
}

std::unique_ptr<Thread> Thread::CreateWebAudioThread() {
  ThreadCreationParams params(WebThreadType::kWebAudioThread);
  // WebAudio uses a thread with |DISPLAY| priority to avoid glitch when the
  // system is under the high pressure. Note that the main browser thread also
  // runs with same priority. (see: crbug.com/734539)
  params.thread_options.priority = base::ThreadPriority::DISPLAY;
  return CreateThread(params);
}

void Thread::CreateAndSetCompositorThread() {
  DCHECK(!GetCompositorThread());

  ThreadCreationParams params(WebThreadType::kCompositorThread);
#if defined(OS_ANDROID)
  params.thread_options.priority = base::ThreadPriority::DISPLAY;
#endif
  auto compositor_thread =
      std::make_unique<scheduler::CompositorThread>(params);
  compositor_thread->Init();
  UpdateThreadTLSAndWait(compositor_thread.get());
  GetCompositorThread() = std::move(compositor_thread);
  Platform::Current()->SetDisplayThreadPriority(
      GetCompositorThread()->ThreadId());
}

Thread* Thread::Current() {
  return ThreadTLSSlot();
}

Thread* Thread::MainThread() {
  return GetMainThread().get();
}

Thread* Thread::CompositorThread() {
  return GetCompositorThread().get();
}

std::unique_ptr<Thread> Thread::SetMainThread(
    std::unique_ptr<Thread> main_thread) {
  ThreadTLSSlot() = main_thread.get();
  std::swap(GetMainThread(), main_thread);
  return main_thread;
}

Thread::Thread() = default;

Thread::~Thread() = default;

bool Thread::IsCurrentThread() const {
  return ThreadTLSSlot() == this;
}

void Thread::AddTaskObserver(TaskObserver* task_observer) {
  CHECK(IsCurrentThread());
  Scheduler()->AddTaskObserver(task_observer);
}

void Thread::RemoveTaskObserver(TaskObserver* task_observer) {
  CHECK(IsCurrentThread());
  Scheduler()->RemoveTaskObserver(task_observer);
}

#if defined(OS_WIN)
static_assert(sizeof(blink::PlatformThreadId) >= sizeof(DWORD),
              "size of platform thread id is too small");
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
static_assert(sizeof(blink::PlatformThreadId) >= sizeof(pid_t),
              "size of platform thread id is too small");
#else
#error Unexpected platform
#endif

}  // namespace blink
