/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/public/platform/platform.h"

#include <memory>

#include "base/allocator/partition_allocator/memory_reclaimer.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/public/platform/web_prerendering_support.h"
#include "third_party/blink/public/platform/websocket_handshake_throttle.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string_manager.h"
#include "third_party/blink/renderer/platform/font_family_names.h"
#include "third_party/blink/renderer/platform/fonts/font_cache_memory_dump_provider.h"
#include "third_party/blink/renderer/platform/heap/blink_gc_memory_dump_provider.h"
#include "third_party/blink/renderer/platform/heap/gc_task_runner.h"
#include "third_party/blink/renderer/platform/instrumentation/instance_counters_memory_dump_provider.h"
#include "third_party/blink/renderer/platform/instrumentation/memory_pressure_listener.h"
#include "third_party/blink/renderer/platform/instrumentation/partition_alloc_memory_dump_provider.h"
#include "third_party/blink/renderer/platform/instrumentation/resource_coordinator/renderer_resource_coordinator.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/memory_cache_dump_provider.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/scheduler/common/simple_thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/webrtc/api/rtp_parameters.h"
#include "third_party/webrtc/p2p/base/port_allocator.h"

namespace blink {

namespace {

class DefaultInterfaceProvider : public InterfaceProvider {
  USING_FAST_MALLOC(DefaultInterfaceProvider);

 public:
  DefaultInterfaceProvider() = default;
  ~DefaultInterfaceProvider() = default;

  // InterfaceProvider implementation:
  void GetInterface(const char* interface_name,
                    mojo::ScopedMessagePipeHandle interface_pipe) override {
    Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
        mojo::GenericPendingReceiver(interface_name,
                                     std::move(interface_pipe)));
  }
};

class DefaultBrowserInterfaceBrokerProxy
    : public ThreadSafeBrowserInterfaceBrokerProxy {
  USING_FAST_MALLOC(DefaultBrowserInterfaceBrokerProxy);

 public:
  DefaultBrowserInterfaceBrokerProxy() = default;

  // ThreadSafeBrowserInterfaceBrokerProxy implementation:
  void GetInterfaceImpl(mojo::GenericPendingReceiver receiver) override {}

 private:
  ~DefaultBrowserInterfaceBrokerProxy() override = default;
};

class IdleDelayedTaskHelper : public base::SingleThreadTaskRunner {
  USING_FAST_MALLOC(IdleDelayedTaskHelper);

 public:
  IdleDelayedTaskHelper() = default;

  bool RunsTasksInCurrentSequence() const override { return IsMainThread(); }

  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override {
    NOTIMPLEMENTED();
    return false;
  }

  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    ThreadScheduler::Current()->PostDelayedIdleTask(
        from_here, delay,
        base::BindOnce([](base::OnceClosure task,
                          base::TimeTicks deadline) { std::move(task).Run(); },
                       std::move(task)));
    return true;
  }

 protected:
  ~IdleDelayedTaskHelper() override = default;

 private:
  THREAD_CHECKER(thread_checker_);
  DISALLOW_COPY_AND_ASSIGN(IdleDelayedTaskHelper);
};

}  // namespace

static Platform* g_platform = nullptr;

static GCTaskRunner* g_gc_task_runner = nullptr;

static void CallOnMainThreadFunction(WTF::MainThreadFunction function,
                                     void* context) {
  PostCrossThreadTask(
      *Thread::MainThread()->GetTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(function, CrossThreadUnretained(context)));
}

Platform::Platform() {
  WTF::Partitions::Initialize();
}

Platform::~Platform() = default;

namespace {

class SimpleMainThread : public Thread {
 public:
  // We rely on base::ThreadTaskRunnerHandle for tasks posted on the main
  // thread. The task runner handle may not be available on Blink's startup
  // (== on SimpleMainThread's construction), because some tests like
  // blink_platform_unittests do not set up a global task environment.
  // In those cases, a task environment is set up on a test fixture's
  // creation, and GetTaskRunner() returns the right task runner during
  // a test.
  //
  // If GetTaskRunner() can be called from a non-main thread (including
  // a worker thread running Mojo callbacks), we need to somehow get a task
  // runner for the main thread. This is not possible with
  // ThreadTaskRunnerHandle. We currently deal with this issue by setting
  // the main thread task runner on the test startup and clearing it on
  // the test tear-down. This is what SetMainThreadTaskRunnerForTesting() for.
  // This function is called from Platform::SetMainThreadTaskRunnerForTesting()
  // and Platform::UnsetMainThreadTaskRunnerForTesting().

  ThreadScheduler* Scheduler() override { return &scheduler_; }
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() const override {
    if (main_thread_task_runner_for_testing_)
      return main_thread_task_runner_for_testing_;
    DCHECK(WTF::IsMainThread());
    return base::ThreadTaskRunnerHandle::Get();
  }

  void SetMainThreadTaskRunnerForTesting(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    main_thread_task_runner_for_testing_ = std::move(task_runner);
  }

 private:
  bool IsSimpleMainThread() const override { return true; }

  scheduler::SimpleThreadScheduler scheduler_;
  scoped_refptr<base::SingleThreadTaskRunner>
      main_thread_task_runner_for_testing_;
};

}  // namespace

void Platform::Initialize(
    Platform* platform,
    scheduler::WebThreadScheduler* main_thread_scheduler) {
  DCHECK(!g_platform);
  DCHECK(platform);
  g_platform = platform;
  InitializeCommon(platform, main_thread_scheduler->CreateMainThread());
}

void Platform::CreateMainThreadAndInitialize(Platform* platform) {
  DCHECK(!g_platform);
  DCHECK(platform);
  g_platform = platform;
  InitializeCommon(platform, std::make_unique<SimpleMainThread>());
}

void Platform::InitializeCommon(Platform* platform,
                                std::unique_ptr<Thread> main_thread) {
  WTF::Initialize(CallOnMainThreadFunction);

  Thread::SetMainThread(std::move(main_thread));

  ProcessHeap::Init();

  ThreadState* thread_state = ThreadState::AttachMainThread();
  new BlinkGCMemoryDumpProvider(
      thread_state, base::ThreadTaskRunnerHandle::Get(),
      BlinkGCMemoryDumpProvider::HeapType::kBlinkMainThread);

  MemoryPressureListenerRegistry::Initialize();

  // font_family_names are used by platform/fonts and are initialized by core.
  // In case core is not available (like on PPAPI plugins), we need to init
  // them here.
  font_family_names::Init();
  InitializePlatformLanguage();

  DCHECK(!g_gc_task_runner);
  g_gc_task_runner = new GCTaskRunner(Thread::MainThread());
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      PartitionAllocMemoryDumpProvider::Instance(), "PartitionAlloc",
      base::ThreadTaskRunnerHandle::Get());
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      FontCacheMemoryDumpProvider::Instance(), "FontCaches",
      base::ThreadTaskRunnerHandle::Get());
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      MemoryCacheDumpProvider::Instance(), "MemoryCache",
      base::ThreadTaskRunnerHandle::Get());
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      InstanceCountersMemoryDumpProvider::Instance(), "BlinkObjectCounters",
      base::ThreadTaskRunnerHandle::Get());
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      ParkableStringManagerDumpProvider::Instance(), "ParkableStrings",
      base::ThreadTaskRunnerHandle::Get());

  RendererResourceCoordinator::MaybeInitialize();
  // Use a delayed idle task as this is low priority work that should stop when
  // the main thread is not doing any work.
  WTF::Partitions::StartPeriodicReclaim(
      base::MakeRefCounted<IdleDelayedTaskHelper>());
}

void Platform::SetCurrentPlatformForTesting(Platform* platform) {
  DCHECK(platform);
  g_platform = platform;
}

void Platform::CreateMainThreadForTesting() {
  DCHECK(!Thread::MainThread());
  Thread::SetMainThread(std::make_unique<SimpleMainThread>());
}

void Platform::SetMainThreadTaskRunnerForTesting() {
  DCHECK(WTF::IsMainThread());
  DCHECK(Thread::MainThread()->IsSimpleMainThread());
  static_cast<SimpleMainThread*>(Thread::MainThread())
      ->SetMainThreadTaskRunnerForTesting(base::ThreadTaskRunnerHandle::Get());
}

void Platform::UnsetMainThreadTaskRunnerForTesting() {
  DCHECK(WTF::IsMainThread());
  DCHECK(Thread::MainThread()->IsSimpleMainThread());
  static_cast<SimpleMainThread*>(Thread::MainThread())
      ->SetMainThreadTaskRunnerForTesting(nullptr);
}

Platform* Platform::Current() {
  return g_platform;
}

InterfaceProvider* Platform::GetInterfaceProvider() {
  DEFINE_STATIC_LOCAL(DefaultInterfaceProvider, provider, ());
  return &provider;
}

ThreadSafeBrowserInterfaceBrokerProxy* Platform::GetBrowserInterfaceBroker() {
  DEFINE_STATIC_LOCAL(DefaultBrowserInterfaceBrokerProxy, proxy, ());
  return &proxy;
}

std::unique_ptr<Thread> Platform::CreateThread(
    const ThreadCreationParams& params) {
  return Thread::CreateThread(params);
}

void Platform::CreateAndSetCompositorThread() {
  Thread::CreateAndSetCompositorThread();
}

scoped_refptr<base::SingleThreadTaskRunner>
Platform::CompositorThreadTaskRunner() {
  if (Thread* compositor_thread = Thread::CompositorThread())
    return compositor_thread->GetTaskRunner();
  return nullptr;
}

std::unique_ptr<WebGraphicsContext3DProvider>
Platform::CreateOffscreenGraphicsContext3DProvider(
    const Platform::ContextAttributes&,
    const WebURL& top_document_url,
    Platform::GraphicsInfo*) {
  return nullptr;
}

std::unique_ptr<WebGraphicsContext3DProvider>
Platform::CreateSharedOffscreenGraphicsContext3DProvider() {
  return nullptr;
}

std::unique_ptr<WebGraphicsContext3DProvider>
Platform::CreateWebGPUGraphicsContext3DProvider(
    const WebURL& top_document_url) {
  return nullptr;
}

}  // namespace blink
