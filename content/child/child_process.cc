// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/child_process.h"

#include <string.h>

#include "base/clang_profiling_buildflags.h"
#include "base/functional/bind.h"
#include "base/message_loop/message_pump_type.h"
#include "base/process/process_handle.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/hang_watcher.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "build/config/compiler/compiler_buildflags.h"
#include "content/child/child_thread_impl.h"
#include "content/common/process_visibility_tracker.h"
#include "mojo/public/cpp/bindings/interface_endpoint_client.h"
#include "sandbox/policy/sandbox_type.h"
#include "services/tracing/public/cpp/trace_startup.h"
#include "third_party/blink/public/common/features.h"

#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
#include "base/test/clang_profiling.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "content/common/android/cpu_time_metrics.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "content/child/sandboxed_process_thread_type_handler.h"
#endif

namespace content {

namespace {

constinit thread_local ChildProcess* child_process = nullptr;

class ChildIOThread : public base::Thread {
 public:
  ChildIOThread() : base::Thread("Chrome_ChildIOThread") {}
  ChildIOThread(const ChildIOThread&) = delete;
  ChildIOThread(ChildIOThread&&) = delete;
  ChildIOThread& operator=(const ChildIOThread&) = delete;
  ChildIOThread& operator=(ChildIOThread&&) = delete;

  void Run(base::RunLoop* run_loop) override {
    mojo::InterfaceEndpointClient::SetThreadNameSuffixForMetrics(
        "ChildIOThread");
    base::ScopedClosureRunner unregister_thread_closure;
    if (base::HangWatcher::IsIOThreadHangWatchingEnabled()) {
      unregister_thread_closure = base::HangWatcher::RegisterThread(
          base::HangWatcher::ThreadType::kIOThread);
    }
    base::Thread::Run(run_loop);
  }
};

}  // namespace

ChildProcess::ChildProcess(base::ThreadType io_thread_type,
                           std::unique_ptr<base::ThreadPoolInstance::InitParams>
                               thread_pool_init_params)
    : resetter_(&child_process, this, nullptr),
      io_thread_(std::make_unique<ChildIOThread>()) {
  // Start ThreadPoolInstance if not already done. A ThreadPoolInstance
  // should already exist, and may already be running when ChildProcess is
  // instantiated in the browser process or in a test process.
  //
  // There are 3 possibilities:
  //
  // 1. ChildProcess is actually being constructed on a thread in the browser
  //    process (eg. for single-process mode). The ThreadPool was already
  //    started on the main thread, but this happened before the ChildProcess
  //    thread was created, which creates a happens-before relationship. So
  //    it's safe to check WasStartedUnsafe().
  // 2. ChildProcess is being constructed in a test. The ThreadPool was
  //    already started by TaskEnvironment on the main thread. Depending on
  //    the test, ChildProcess might be constructed on the main thread or
  //    another thread that was created after the test start. Either way, it's
  //    safe to check WasStartedUnsafe().
  // 3. ChildProcess is being constructed in a subprocess from ContentMain, on
  //    the main thread. This is the same thread that created the ThreadPool
  //    so it's safe to check WasStartedUnsafe().
  //
  // Note that the only case we expect WasStartedUnsafe() to return true
  // should be running on the main thread. So if there's a logic error and a
  // stale read causes WasStartedUnsafe() to return false after the
  // ThreadPool was started, Start() will correctly DCHECK as it's called on the
  // wrong thread. (The result never flips from true to false so a stale read
  // should never return true.)
  auto* thread_pool = base::ThreadPoolInstance::Get();
  DCHECK(thread_pool);
  if (!thread_pool->WasStartedUnsafe()) {
    if (thread_pool_init_params)
      thread_pool->Start(*thread_pool_init_params.get());
    else
      thread_pool->StartWithDefaultParams();
    initialized_thread_pool_ = true;
  }

  tracing::InitTracingPostThreadPoolStartAndFeatureList(
      /* enable_consumer */ false);

  // Ensure the visibility tracker is created on the main thread.
  ProcessVisibilityTracker::GetInstance();

#if BUILDFLAG(IS_ANDROID)
  SetupCpuTimeMetrics();
#endif

  // We can't recover from failing to start the IO thread.
  base::Thread::Options thread_options(base::MessagePumpType::IO, 0);
  thread_options.thread_type = io_thread_type;
// TODO(crbug.com/40226692): Figure out whether IS_ANDROID can be lifted here.
#if BUILDFLAG(IS_ANDROID)
  // TODO(reveman): Remove this in favor of setting it explicitly for each type
  // of process.
  thread_options.thread_type = base::ThreadType::kDisplayCritical;
#endif
  CHECK(io_thread_->StartWithOptions(std::move(thread_options)));
  io_thread_runner_ = io_thread_->task_runner();
}

ChildProcess::ChildProcess(
    scoped_refptr<base::SingleThreadTaskRunner> io_thread_runner)
    : resetter_(&child_process, this, nullptr),
      io_thread_runner_(std::move(io_thread_runner)) {}

ChildProcess::~ChildProcess() {
  DCHECK_EQ(child_process, this);

  // Signal this event before destroying the child process.  That way all
  // background threads can cleanup.
  // For example, in the renderer the RenderThread instances will be able to
  // notice shutdown before the render process begins waiting for them to exit.
  shutdown_event_.Signal();

  if (main_thread_) {  // null in unittests.
    main_thread_->Shutdown();
    if (main_thread_->ShouldBeDestroyed()) {
      main_thread_.reset();
    } else {
      // Leak the main_thread_. See a comment in
      // RenderThreadImpl::ShouldBeDestroyed.
      main_thread_.release();
    }
  }

  if (io_thread_) {
    io_thread_->Stop();
    io_thread_.reset();
  }

  if (initialized_thread_pool_) {
    DCHECK(base::ThreadPoolInstance::Get());
    base::ThreadPoolInstance::Get()->Shutdown();
  }

#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX) && BUILDFLAG(CLANG_PGO)
  // Flush the profiling data to disk. Doing this manually (vs relying on this
  // being done automatically when the process exits) will ensure that this data
  // doesn't get lost if the process is fast killed.
  base::WriteClangProfilingProfile();
#endif
}

ChildThreadImpl* ChildProcess::main_thread() {
  return main_thread_.get();
}

void ChildProcess::set_main_thread(ChildThreadImpl* thread) {
  main_thread_.reset(thread);
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
void ChildProcess::SetIOThreadType(base::ThreadType thread_type) {
  if (!io_thread_) {
    return;
  }

  // The SandboxedProcessThreadTypeHandler isn't created in
  // in --single-process mode or if certain base::Features are disabled. See
  // instances of SandboxedProcessThreadTypeHandler::Create() for more details.
  if (SandboxedProcessThreadTypeHandler* sandboxed_process_thread_type_handler =
          SandboxedProcessThreadTypeHandler::Get()) {
    sandboxed_process_thread_type_handler->HandleThreadTypeChange(
        io_thread_->GetThreadId(), base::ThreadType::kDisplayCritical);
  }
}
#endif

void ChildProcess::AddRefProcess() {
  DCHECK(!main_thread_.get() ||  // null in unittests.
         main_thread_->main_thread_runner()->BelongsToCurrentThread());
  ref_count_++;
}

void ChildProcess::ReleaseProcess() {
  DCHECK(!main_thread_.get() ||  // null in unittests.
         main_thread_->main_thread_runner()->BelongsToCurrentThread());
  DCHECK(ref_count_);
  if (--ref_count_)
    return;

  if (main_thread_)  // null in unittests.
    main_thread_->OnProcessFinalRelease();
}

ChildProcess* ChildProcess::current() {
  return child_process;
}

base::WaitableEvent* ChildProcess::GetShutDownEvent() {
  return &shutdown_event_;
}

}  // namespace content
