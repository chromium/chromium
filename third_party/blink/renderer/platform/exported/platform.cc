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

#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "media/base/media_log.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_fetch_handler_bypass_option.mojom-blink-forward.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/platform/web_dedicated_worker_host_factory_client.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/public/platform/websocket_handshake_throttle.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string_manager.h"
#include "third_party/blink/renderer/platform/font_family_names.h"
#include "third_party/blink/renderer/platform/fonts/font_cache_memory_dump_provider.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/graphics/parkable_image_manager.h"
#include "third_party/blink/renderer/platform/heap/blink_gc_memory_dump_provider.h"
#include "third_party/blink/renderer/platform/heap/process_heap.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/instrumentation/canvas_memory_dump_provider.h"
#include "third_party/blink/renderer/platform/instrumentation/instance_counters_memory_dump_provider.h"
#include "third_party/blink/renderer/platform/instrumentation/memory_pressure_listener.h"
#include "third_party/blink/renderer/platform/instrumentation/partition_alloc_memory_dump_provider.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/memory_cache_dump_provider.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader_factory.h"
#include "third_party/blink/renderer/platform/scheduler/common/simple_main_thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/dummy_schedulers.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/non_main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/theme/web_theme_engine_helper.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/webrtc/api/rtp_parameters.h"
#include "third_party/webrtc/p2p/base/port_allocator.h"

namespace blink {

namespace {

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
  IdleDelayedTaskHelper(const IdleDelayedTaskHelper&) = delete;
  IdleDelayedTaskHelper& operator=(const IdleDelayedTaskHelper&) = delete;

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
};

}  // namespace

static Platform* g_platform = nullptr;

static bool did_initialize_blink_ = false;

Platform::Platform() = default;

Platform::~Platform() = default;

WebThemeEngine* Platform::ThemeEngine() {
  return WebThemeEngineHelper::GetNativeThemeEngine();
}

void Platform::InitializeBlink() {
  DCHECK(!did_initialize_blink_);
  WTF::Partitions::Initialize();
  WTF::Initialize();
  Length::Initialize();
  ProcessHeap::Init();
  ThreadState::AttachMainThread();
  did_initialize_blink_ = true;
}

void Platform::InitializeMainThread(
    Platform* platform,
    scheduler::WebThreadScheduler* main_thread_scheduler) {
  DCHECK(!g_platform);
  DCHECK(platform);
  g_platform = platform;
  InitializeMainThreadCommon(platform,
                             main_thread_scheduler->CreateMainThread());
}

void Platform::CreateMainThreadAndInitialize(Platform* platform) {
  DCHECK(!g_platform);
  DCHECK(platform);
  g_platform = platform;
  InitializeBlink();
  InitializeMainThreadCommon(platform, scheduler::CreateSimpleMainThread());
}

void Platform::InitializeMainThreadCommon(
    Platform* platform,
    std::unique_ptr<MainThread> main_thread) {
  DCHECK(did_initialize_blink_);
  MainThread::SetMainThread(std::move(main_thread));

  ThreadState* thread_state = ThreadState::Current();
  CHECK(thread_state->IsMainThread());
  new BlinkGCMemoryDumpProvider(
      thread_state, base::SingleThreadTaskRunner::GetCurrentDefault(),
      BlinkGCMemoryDumpProvider::HeapType::kBlinkMainThread);

  MemoryPressureListenerRegistry::Initialize();

  // font_family_names are used by platform/fonts and are initialized by core.
  // In case core is not available (like on PPAPI plugins), we need to init
  // them here.
  font_family_names::Init();
  InitializePlatformLanguage();

  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      PartitionAllocMemoryDumpProvider::Instance(), "PartitionAlloc",
      base::SingleThreadTaskRunner::GetCurrentDefault());
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      FontCacheMemoryDumpProvider::Instance(), "FontCaches",
      base::SingleThreadTaskRunner::GetCurrentDefault());
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      MemoryCacheDumpProvider::Instance(), "MemoryCache",
      base::SingleThreadTaskRunner::GetCurrentDefault());
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      InstanceCountersMemoryDumpProvider::Instance(), "BlinkObjectCounters",
      base::SingleThreadTaskRunner::GetCurrentDefault());
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      ParkableStringManagerDumpProvider::Instance(), "ParkableStrings",
      base::SingleThreadTaskRunner::GetCurrentDefault());
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      &ParkableImageManager::Instance(), "ParkableImages",
      base::SingleThreadTaskRunner::GetCurrentDefault());
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      CanvasMemoryDumpProvider::Instance(), "Canvas",
      base::SingleThreadTaskRunner::GetCurrentDefault());

  // Use a delayed idle task as this is low priority work that should stop when
  // the main thread is not doing any work.
  //
  // This relies on being called prior to
  // PartitionAllocSupport::ReconfigureAfterTaskRunnerInit, which would start
  // memory reclaimer with a regular task runner. The first one prevails.
  WTF::Partitions::StartMemoryReclaimer(
      base::MakeRefCounted<IdleDelayedTaskHelper>());
}

void Platform::SetCurrentPlatformForTesting(Platform* platform) {
  DCHECK(platform);
  g_platform = platform;
}

void Platform::CreateMainThreadForTesting() {
  DCHECK(!Thread::MainThread());
  MainThread::SetMainThread(scheduler::CreateSimpleMainThread());
}

void Platform::SetMainThreadTaskRunnerForTesting() {
  DCHECK(WTF::IsMainThread());
  DCHECK(Thread::MainThread()->IsSimpleMainThread());
  scheduler::SetMainThreadTaskRunnerForTesting();
}

void Platform::UnsetMainThreadTaskRunnerForTesting() {
  DCHECK(WTF::IsMainThread());
  DCHECK(Thread::MainThread()->IsSimpleMainThread());
  scheduler::UnsetMainThreadTaskRunnerForTesting();
}

Platform* Platform::Current() {
  return g_platform;
}

std::unique_ptr<WebDedicatedWorkerHostFactoryClient>
Platform::CreateDedicatedWorkerHostFactoryClient(
    WebDedicatedWorker*,
    const BrowserInterfaceBrokerProxy&) {
  return nullptr;
}

void Platform::CreateServiceWorkerSubresourceLoaderFactory(
    CrossVariantMojoRemote<mojom::ServiceWorkerContainerHostInterfaceBase>
        service_worker_container_host,
    const WebString& client_id,
    std::unique_ptr<network::PendingSharedURLLoaderFactory> fallback_factory,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {}

ThreadSafeBrowserInterfaceBrokerProxy* Platform::GetBrowserInterfaceBroker() {
  DEFINE_STATIC_LOCAL(DefaultBrowserInterfaceBrokerProxy, proxy, ());
  return &proxy;
}

void Platform::CreateAndSetCompositorThread() {
  Thread::CreateAndSetCompositorThread();
}

scoped_refptr<base::SingleThreadTaskRunner>
Platform::CompositorThreadTaskRunner() {
  if (NonMainThread* compositor_thread = Thread::CompositorThread())
    return compositor_thread->GetTaskRunner();
  return nullptr;
}

std::unique_ptr<WebGraphicsContext3DProvider>
Platform::CreateOffscreenGraphicsContext3DProvider(
    const Platform::ContextAttributes&,
    const WebURL& document_url,
    Platform::GraphicsInfo*) {
  return nullptr;
}

std::unique_ptr<WebGraphicsContext3DProvider>
Platform::CreateSharedOffscreenGraphicsContext3DProvider() {
  return nullptr;
}

std::unique_ptr<WebGraphicsContext3DProvider>
Platform::CreateWebGPUGraphicsContext3DProvider(const WebURL& document_url) {
  return nullptr;
}

void Platform::CreateWebGPUGraphicsContext3DProviderAsync(
    const blink::WebURL& document_url,
    base::OnceCallback<
        void(std::unique_ptr<blink::WebGraphicsContext3DProvider>)> callback) {}

scoped_refptr<viz::RasterContextProvider>
Platform::SharedMainThreadContextProvider() {
  return nullptr;
}

scoped_refptr<cc::RasterContextProviderWrapper>
Platform::SharedCompositorWorkerContextProvider(
    cc::RasterDarkModeFilter* dark_mode_filter) {
  return nullptr;
}

scoped_refptr<gpu::GpuChannelHost> Platform::EstablishGpuChannelSync() {
  return nullptr;
}

bool Platform::IsGpuRemoteDisconnected() {
  return false;
}

void Platform::EstablishGpuChannel(EstablishGpuChannelCallback callback) {
  std::move(callback).Run(nullptr);
}

gfx::ColorSpace Platform::GetRenderingColorSpace() const {
  return {};
}

std::unique_ptr<media::MediaLog> Platform::GetMediaLog(
    MediaInspectorContext* inspector_context,
    scoped_refptr<base::SingleThreadTaskRunner> owner_task_runner,
    bool is_on_worker) {
  return nullptr;
}

size_t Platform::GetMaxDecodedImageBytes() {
  return Current() ? Current()->MaxDecodedImageBytes()
                   : kNoDecodedImageByteLimit;
}

}  // namespace blink
