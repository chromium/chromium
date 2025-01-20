/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/controller/blink_initializer.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "partition_alloc/page_allocator.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/platform/interface_registry.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_context_snapshot.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_initializer.h"
#include "third_party/blink/renderer/controller/blink_leak_detector.h"
#include "third_party/blink/renderer/controller/dev_tools_frontend_impl.h"
#include "third_party/blink/renderer/controller/javascript_call_stack_generator.h"
#include "third_party/blink/renderer/controller/performance_manager/renderer_resource_coordinator_impl.h"
#include "third_party/blink/renderer/controller/performance_manager/v8_detailed_memory_reporter_impl.h"
#include "third_party/blink/renderer/core/animation/animation_clock.h"
#include "third_party/blink/renderer/core/annotation/annotation_agent_container_impl.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/display_cutout_client_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/loader_factory_for_frame.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/disk_data_allocator.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"
#include "v8/include/v8.h"

#if defined(USE_BLINK_EXTENSIONS_CHROMEOS)
#include "third_party/blink/renderer/extensions/chromeos/chromeos_extensions.h"
#endif

#if defined(USE_BLINK_EXTENSIONS_WEBVIEW)
#include "third_party/blink/renderer/extensions/webview/webview_extensions.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "third_party/blink/renderer/controller/crash_memory_metrics_reporter_impl.h"
#include "third_party/blink/renderer/controller/oom_intervention_impl.h"
#include "third_party/blink/renderer/controller/private_memory_footprint_provider.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "third_party/blink/renderer/controller/memory_usage_monitor_posix.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN)
#include "third_party/blink/renderer/controller/highest_pmf_reporter.h"
#include "third_party/blink/renderer/controller/user_level_memory_pressure_signal_generator.h"
#endif

// #if expression should match the one in InitializeCommon
#if !defined(ARCH_CPU_X86_64) && !defined(ARCH_CPU_ARM64) && BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace blink {

namespace {

class EndOfTaskRunner : public Thread::TaskObserver {
 public:
  void WillProcessTask(const base::PendingTask&, bool) override {
    AnimationClock::NotifyTaskStart();
  }
  void DidProcessTask(const base::PendingTask& pending_task) override {}
};

Thread::TaskObserver* g_end_of_task_runner = nullptr;

BlinkInitializer& GetBlinkInitializer() {
  DEFINE_STATIC_LOCAL(std::unique_ptr<BlinkInitializer>, initializer,
                      (std::make_unique<BlinkInitializer>()));
  return *initializer;
}

void InitializeCommon(Platform* platform, mojo::BinderMap* binders) {
// #if expression should match the one around #include <windows.h>
#if !defined(ARCH_CPU_X86_64) && !defined(ARCH_CPU_ARM64) && BUILDFLAG(IS_WIN)
  // Reserve address space on 32 bit Windows, to make it likelier that large
  // array buffer allocations succeed.
  BOOL is_wow_64 = -1;
  if (!IsWow64Process(GetCurrentProcess(), &is_wow_64)) {
    is_wow_64 = FALSE;
  }
  if (!is_wow_64) {
    // Try to reserve as much address space as we reasonably can.
    const size_t kMB = 1024 * 1024;
    for (size_t size = 512 * kMB; size >= 32 * kMB; size -= 16 * kMB) {
      if (partition_alloc::ReserveAddressSpace(size)) {
        break;
      }
    }
  }
#endif  // !defined(ARCH_CPU_X86_64) && !defined(ARCH_CPU_ARM64) &&
        // BUILDFLAG(IS_WIN)

  // These Initialize() methods for renderer extensions initialize strings which
  // must be done before calling CoreInitializer::Initialize() which is called
  // by GetBlinkInitializer().Initialize() below.
#if defined(USE_BLINK_EXTENSIONS_CHROMEOS)
  ChromeOSExtensions::Initialize();
#endif
#if defined(USE_BLINK_EXTENSIONS_WEBVIEW)
  WebViewExtensions::Initialize();
#endif

  // BlinkInitializer::Initialize() must be called before InitializeMainThread
  GetBlinkInitializer().Initialize();

  blink::V8Initializer::InitializeIsolateHolder(
      blink::V8ContextSnapshot::GetReferenceTable(),
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          blink::switches::kJavaScriptFlags));

  GetBlinkInitializer().RegisterInterfaces(*binders);

  DCHECK(!g_end_of_task_runner);
  g_end_of_task_runner = new EndOfTaskRunner;
  Thread::Current()->AddTaskObserver(g_end_of_task_runner);

  GetBlinkInitializer().RegisterMemoryWatchers(platform);

  // Initialize performance manager.
  RendererResourceCoordinatorImpl::MaybeInitialize();

  // The ArrayBuffer partition is placed inside V8's virtual memory cage if it
  // is enabled. For that reason, the partition can only be initialized after V8
  // has been initialized.
  WTF::Partitions::InitializeArrayBufferPartition();
}

}  // namespace

// Function defined in third_party/blink/public/web/blink.h.
void Initialize(Platform* platform,
                mojo::BinderMap* binders,
                scheduler::WebThreadScheduler* main_thread_scheduler) {
  DCHECK(binders);
  Platform::InitializeMainThread(platform, main_thread_scheduler);
  InitializeCommon(platform, binders);
  V8Initializer::InitializeMainThread();
}

// Function defined in third_party/blink/public/web/blink.h.
void CreateMainThreadAndInitialize(Platform* platform,
                                   mojo::BinderMap* binders) {
  DCHECK(binders);
  Platform::CreateMainThreadAndInitialize(platform);
  InitializeCommon(platform, binders);
}

void InitializeWithoutIsolateForTesting(
    Platform* platform,
    mojo::BinderMap* binders,
    scheduler::WebThreadScheduler* main_thread_scheduler) {
  Platform::InitializeMainThread(platform, main_thread_scheduler);
  InitializeCommon(platform, binders);
}

v8::Isolate* CreateMainThreadIsolate() {
  return V8Initializer::InitializeMainThread();
}

// Function defined in third_party/blink/public/web/blink.h.
void SetIsCrossOriginIsolated(bool value) {
  Agent::SetIsCrossOriginIsolated(value);
}

// Function defined in third_party/blink/public/web/blink.h.
void SetIsWebSecurityDisabled(bool value) {
  Agent::SetIsWebSecurityDisabled(value);
}

// Function defined in third_party/blink/public/web/blink.h.
void SetIsIsolatedContext(bool value) {
  Agent::SetIsIsolatedContext(value);
}

// Function defined in third_party/blink/public/web/blink.h.
bool IsIsolatedContext() {
  return Agent::IsIsolatedContext();
}

// Function defined in third_party/blink/public/web/blink.h.
void SetCorsExemptHeaderList(
    const WebVector<WebString>& web_cors_exempt_header_list) {
  Vector<String> cors_exempt_header_list(
      base::checked_cast<wtf_size_t>(web_cors_exempt_header_list.size()));
  base::ranges::transform(web_cors_exempt_header_list,
                          cors_exempt_header_list.begin(),
                          &WebString::operator WTF::String);
  LoaderFactoryForFrame::SetCorsExemptHeaderList(
      std::move(cors_exempt_header_list));
}

void BlinkInitializer::RegisterInterfaces(mojo::BinderMap& binders) {
  ModulesInitializer::RegisterInterfaces(binders);
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner =
      Thread::MainThread()->GetTaskRunner(MainThreadTaskRunnerRestricted());
  CHECK(main_thread_task_runner);

#if BUILDFLAG(IS_ANDROID)
  binders.Add<mojom::blink::OomIntervention>(
      ConvertToBaseRepeatingCallback(
          CrossThreadBindRepeating(&OomInterventionImpl::BindReceiver,
                                   WTF::RetainedRef(main_thread_task_runner))),
      main_thread_task_runner);

  binders.Add<mojom::blink::CrashMemoryMetricsReporter>(
      ConvertToBaseRepeatingCallback(
          CrossThreadBindRepeating(&CrashMemoryMetricsReporterImpl::Bind)),
      main_thread_task_runner);
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  binders.Add<mojom::blink::MemoryUsageMonitorLinux>(
      ConvertToBaseRepeatingCallback(
          CrossThreadBindRepeating(&MemoryUsageMonitorPosix::Bind)),
      main_thread_task_runner);
#endif

  binders.Add<mojom::blink::LeakDetector>(
      ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
          &BlinkLeakDetector::Bind, WTF::RetainedRef(main_thread_task_runner))),
      main_thread_task_runner);

  binders.Add<mojom::blink::DiskAllocator>(
      ConvertToBaseRepeatingCallback(
          CrossThreadBindRepeating(&DiskDataAllocator::Bind)),
      main_thread_task_runner);

  binders.Add<mojom::blink::V8DetailedMemoryReporter>(
      ConvertToBaseRepeatingCallback(
          CrossThreadBindRepeating(&V8DetailedMemoryReporterImpl::Bind)),
      main_thread_task_runner);

    DCHECK(Platform::Current());
    // We need to use the IO task runner here because the call stack generator
    // should work even when the main thread is blocked.
    binders.Add<mojom::blink::CallStackGenerator>(
        ConvertToBaseRepeatingCallback(
            CrossThreadBindRepeating(&JavaScriptCallStackGenerator::Bind)),
        Platform::Current()->GetIOTaskRunner());
}

void BlinkInitializer::RegisterMemoryWatchers(Platform* platform) {
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner =
      Thread::MainThread()->GetTaskRunner(MainThreadTaskRunnerRestricted());
#if BUILDFLAG(IS_ANDROID)
  // Initialize CrashMemoryMetricsReporterImpl in order to assure that memory
  // allocation does not happen in OnOOMCallback.
  CrashMemoryMetricsReporterImpl::Instance();

  // Initialize UserLevelMemoryPressureSignalGenerator so it starts monitoring.
  if (platform->IsUserLevelMemoryPressureSignalEnabled()) {
    UserLevelMemoryPressureSignalGenerator::Initialize(platform,
                                                       main_thread_task_runner);
  }
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN)
  // Start reporting the highest private memory footprint after the first
  // navigation.
  HighestPmfReporter::Initialize(main_thread_task_runner);
#endif

#if BUILDFLAG(IS_ANDROID)
  // Initialize PrivateMemoryFootprintProvider to start providing the value
  // for the browser process.
  PrivateMemoryFootprintProvider::Initialize(main_thread_task_runner);
#endif
}

void BlinkInitializer::InitLocalFrame(LocalFrame& frame) const {
  if (RuntimeEnabledFeatures::DisplayCutoutAPIEnabled()) {
    frame.GetInterfaceRegistry()->AddAssociatedInterface(
        WTF::BindRepeating(&DisplayCutoutClientImpl::BindMojoReceiver,
                           WrapWeakPersistent(&frame)));
  }
  frame.GetInterfaceRegistry()->AddAssociatedInterface(WTF::BindRepeating(
      &DevToolsFrontendImpl::BindMojoRequest, WrapWeakPersistent(&frame)));

  frame.GetInterfaceRegistry()->AddInterface(WTF::BindRepeating(
      &LocalFrame::PauseSubresourceLoading, WrapWeakPersistent(&frame)));

  frame.GetInterfaceRegistry()->AddInterface(WTF::BindRepeating(
      &AnnotationAgentContainerImpl::BindReceiver, WrapWeakPersistent(&frame)));
  ModulesInitializer::InitLocalFrame(frame);
}

void BlinkInitializer::InitServiceWorkerGlobalScope(
    ServiceWorkerGlobalScope& worker_global_scope) const {
#if defined(USE_BLINK_EXTENSIONS_CHROMEOS)
  ChromeOSExtensions::InitServiceWorkerGlobalScope(worker_global_scope);
#endif
}

void BlinkInitializer::OnClearWindowObjectInMainWorld(
    Document& document,
    const Settings& settings) const {
  if (DevToolsFrontendImpl* devtools_frontend =
          DevToolsFrontendImpl::From(document.GetFrame())) {
    devtools_frontend->DidClearWindowObject();
  }
  ModulesInitializer::OnClearWindowObjectInMainWorld(document, settings);
}

// Function defined in third_party/blink/public/web/blink.h.
void OnProcessForegrounded() {
  WTF::Partitions::AdjustPartitionsForForeground();
}

// Function defined in third_party/blink/public/web/blink.h.
void OnProcessBackgrounded() {
  WTF::Partitions::AdjustPartitionsForBackground();
}

}  // namespace blink
