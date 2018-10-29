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

#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/public/platform/modules/webmidi/web_midi_accessor.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/platform/web_canvas_capture_handler.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/public/platform/web_image_capture_frame_grabber.h"
#include "third_party/blink/public/platform/web_media_recorder_handler.h"
#include "third_party/blink/public/platform/web_media_stream_center.h"
#include "third_party/blink/public/platform/web_prerendering_support.h"
#include "third_party/blink/public/platform/web_rtc_certificate_generator.h"
#include "third_party/blink/public/platform/web_rtc_peer_connection_handler.h"
#include "third_party/blink/public/platform/web_storage_namespace.h"
#include "third_party/blink/public/platform/websocket_handshake_throttle.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/font_family_names.h"
#include "third_party/blink/renderer/platform/fonts/font_cache_memory_dump_provider.h"
#include "third_party/blink/renderer/platform/heap/blink_gc_memory_dump_provider.h"
#include "third_party/blink/renderer/platform/heap/gc_task_runner.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/instance_counters_memory_dump_provider.h"
#include "third_party/blink/renderer/platform/instrumentation/resource_coordinator/renderer_resource_coordinator.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/memory_cache_dump_provider.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/memory_coordinator.h"
#include "third_party/blink/renderer/platform/partition_alloc_memory_dump_provider.h"
#include "third_party/blink/renderer/platform/scheduler/common/simple_thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/web_task_runner.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/webrtc/api/rtpparameters.h"
#include "third_party/webrtc/p2p/base/portallocator.h"

namespace blink {

namespace {

class DefaultConnector {
 public:
  DefaultConnector() {
    service_manager::mojom::ConnectorRequest request;
    connector_ = service_manager::Connector::Create(&request);
  }

  service_manager::Connector* Get() { return connector_.get(); }

 private:
  std::unique_ptr<service_manager::Connector> connector_;
};

}  // namespace

static Platform* g_platform = nullptr;

static GCTaskRunner* g_gc_task_runner = nullptr;

static void MaxObservedSizeFunction(size_t size_in_mb) {
  const size_t kSupportedMaxSizeInMB = 4 * 1024;
  if (size_in_mb >= kSupportedMaxSizeInMB)
    size_in_mb = kSupportedMaxSizeInMB - 1;

  // Send a UseCounter only when we see the highest memory usage
  // we've ever seen.
  DEFINE_STATIC_LOCAL(EnumerationHistogram, committed_size_histogram,
                      ("PartitionAlloc.CommittedSize", kSupportedMaxSizeInMB));
  committed_size_histogram.Count(size_in_mb);
}

static void CallOnMainThreadFunction(WTF::MainThreadFunction function,
                                     void* context) {
  PostCrossThreadTask(
      *Platform::Current()->MainThread()->GetTaskRunner(), FROM_HERE,
      CrossThreadBind(function, CrossThreadUnretained(context)));
}

Platform::Platform() {
  WTF::Partitions::Initialize(MaxObservedSizeFunction);
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
  MemoryCoordinator::Initialize();
  if (base::ThreadTaskRunnerHandle::IsSet()) {
    base::trace_event::MemoryDumpProvider::Options options;
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        BlinkGCMemoryDumpProvider::Instance(), "BlinkGC",
        base::ThreadTaskRunnerHandle::Get(), options);
  }

  ThreadState::AttachMainThread();

  // FontFamilyNames are used by platform/fonts and are initialized by core.
  // In case core is not available (like on PPAPI plugins), we need to init
  // them here.
  FontFamilyNames::init();
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

  RendererResourceCoordinator::Initialize();
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

Thread* Platform::MainThread() {
  return Thread::MainThread();
}

Thread* Platform::CurrentThread() {
  return Thread::Current();
}

service_manager::Connector* Platform::GetConnector() {
  DEFINE_STATIC_LOCAL(DefaultConnector, connector, ());
  return connector.Get();
}

InterfaceProvider* Platform::GetInterfaceProvider() {
  return InterfaceProvider::GetEmptyInterfaceProvider();
}

std::unique_ptr<WebMIDIAccessor> Platform::CreateMIDIAccessor(
    WebMIDIAccessorClient*) {
  return nullptr;
}

std::unique_ptr<WebStorageNamespace> Platform::CreateLocalStorageNamespace() {
  return nullptr;
}

std::unique_ptr<WebStorageNamespace> Platform::CreateSessionStorageNamespace(
    base::StringPiece namespace_id) {
  return nullptr;
}

std::unique_ptr<Thread> Platform::CreateThread(
    const ThreadCreationParams& params) {
  return Thread::CreateThread(params);
}

std::unique_ptr<Thread> Platform::CreateWebAudioThread() {
  return Thread::CreateWebAudioThread();
}

void Platform::CreateAndSetCompositorThread() {
  Thread::CreateAndSetCompositorThread();
}

Thread* Platform::CompositorThread() {
  return Thread::CompositorThread();
}

scoped_refptr<base::SingleThreadTaskRunner>
Platform::CompositorThreadTaskRunner() {
  if (Thread* compositor_thread = CompositorThread())
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
Platform::CreateWebGPUGraphicsContext3DProvider(const WebURL& top_document_url,
                                                GraphicsInfo*) {
  return nullptr;
}

std::unique_ptr<WebRTCPeerConnectionHandler>
Platform::CreateRTCPeerConnectionHandler(
    WebRTCPeerConnectionHandlerClient*,
    scoped_refptr<base::SingleThreadTaskRunner>) {
  return nullptr;
}

std::unique_ptr<cricket::PortAllocator> Platform::CreateWebRtcPortAllocator(
    WebLocalFrame* frame) {
  return nullptr;
}

std::unique_ptr<WebMediaRecorderHandler> Platform::CreateMediaRecorderHandler(
    scoped_refptr<base::SingleThreadTaskRunner>) {
  return nullptr;
}

std::unique_ptr<WebRTCCertificateGenerator>
Platform::CreateRTCCertificateGenerator() {
  return nullptr;
}

std::unique_ptr<WebMediaStreamCenter> Platform::CreateMediaStreamCenter() {
  return nullptr;
}

std::unique_ptr<WebCanvasCaptureHandler> Platform::CreateCanvasCaptureHandler(
    const WebSize&,
    double,
    WebMediaStreamTrack*) {
  return nullptr;
}

std::unique_ptr<WebImageCaptureFrameGrabber>
Platform::CreateImageCaptureFrameGrabber() {
  return nullptr;
}

std::unique_ptr<webrtc::RtpCapabilities> Platform::GetRtpSenderCapabilities(
    const WebString& kind) {
  return nullptr;
}

std::unique_ptr<webrtc::RtpCapabilities> Platform::GetRtpReceiverCapabilities(
    const WebString& kind) {
  return nullptr;
}

}  // namespace blink
