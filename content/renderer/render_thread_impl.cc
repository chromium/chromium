// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_thread_impl.h"

#include <algorithm>
#include <limits>
#include <map>
#include <utility>
#include <vector>

#include "base/allocator/allocator_extension.h"
#include "base/at_exit.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/discardable_memory_allocator.h"
#include "base/message_loop/message_pump_type.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/path_service.h"
#include "base/process/process_metrics.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "build/build_config.h"
#include "cc/base/histograms.h"
#include "cc/base/switches.h"
#include "cc/mojo_embedder/async_layer_tree_frame_sink.h"
#include "cc/raster/task_graph_runner.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/ukm_manager.h"
#include "components/discardable_memory/client/client_discardable_shared_memory_manager.h"
#include "components/metrics/public/mojom/single_sample_metrics.mojom.h"
#include "components/metrics/single_sample_metrics.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/switches.h"
#include "content/child/runtime_features.h"
#include "content/common/buildflags.h"
#include "content/common/content_constants_internal.h"
#include "content/common/frame_messages.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/gpu_stream_constants.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/render_thread_observer.h"
#include "content/public/renderer/render_view_visitor.h"
#include "content/renderer/agent_scheduling_group.h"
#include "content/renderer/browser_exposed_renderer_interfaces.h"
#include "content/renderer/categorized_worker_pool.h"
#include "content/renderer/effective_connection_type_helper.h"
#include "content/renderer/loader/resource_dispatcher.h"
#include "content/renderer/media/gpu/gpu_video_accelerator_factories_impl.h"
#include "content/renderer/media/render_media_client.h"
#include "content/renderer/net_info_helper.h"
#include "content/renderer/render_frame_metadata_observer_impl.h"
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/render_process_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/renderer_blink_platform_impl.h"
#include "content/renderer/service_worker/service_worker_context_client.h"
#include "content/renderer/variations_render_thread_observer.h"
#include "content/renderer/worker/embedded_shared_worker_stub.h"
#include "content/renderer/worker/worker_thread_registry.h"
#include "device/gamepad/public/cpp/gamepads.h"
#include "gin/public/debug.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_channel_mojo.h"
#include "ipc/ipc_platform_file.h"
#include "media/base/media.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "net/base/net_errors.h"
#include "net/base/port_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "services/viz/public/cpp/gpu/gpu.h"
#include "skia/ext/skia_memory_dump_provider.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/platform/modules/video_capture/web_video_capture_impl_manager.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/platform/web_cache.h"
#include "third_party/blink/public/platform/web_image_generator.h"
#include "third_party/blink/public/platform/web_memory_pressure_listener.h"
#include "third_party/blink/public/platform/web_network_state_notifier.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/platform/web_scoped_page_pauser.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_render_theme.h"
#include "third_party/blink/public/web/web_script_controller.h"
#include "third_party/blink/public/web/web_security_policy.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "ui/base/layout.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/display_switches.h"

#if defined(OS_ANDROID)
#include <cpu-features.h>
#include "content/renderer/android/synchronous_layer_tree_frame_sink_impl.h"
#include "content/renderer/media/android/stream_texture_factory.h"
#include "media/base/android/media_codec_util.h"
#endif

#if defined(OS_MAC)
#include "base/mac/mac_util.h"
#include "content/renderer/theme_helper_mac.h"
#endif

#if defined(OS_WIN)
#include <objbase.h>
#include <windows.h>
#endif

#ifdef ENABLE_VTUNE_JIT_INTERFACE
#include "v8/src/third_party/vtune/v8-vtune.h"
#endif

#if defined(ENABLE_IPC_FUZZER)
#include "content/common/external_ipc_dumper.h"
#include "mojo/public/cpp/bindings/message_dumper.h"
#endif

#if defined(OS_MAC)
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif

#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
#include "base/test/clang_profiling.h"
#endif

namespace content {

namespace {

using ::base::ThreadRestrictions;
using ::blink::WebDocument;
using ::blink::WebFrame;
using ::blink::WebNetworkStateNotifier;
using ::blink::WebRuntimeFeatures;
using ::blink::WebScriptController;
using ::blink::WebSecurityPolicy;
using ::blink::WebString;
using ::blink::WebView;
using ::util::PassKey;

#if defined(OS_ANDROID)
// Unique identifier for each output surface created.
uint32_t g_next_layer_tree_frame_sink_id = 1;
#endif

// An implementation of mojom::RenderMessageFilter which can be mocked out
// for tests which may indirectly send messages over this interface.
mojom::RenderMessageFilter* g_render_message_filter_for_testing;

// An implementation of RendererBlinkPlatformImpl which can be mocked out
// for tests.
RendererBlinkPlatformImpl* g_current_blink_platform_impl_for_testing;

// Keep the global RenderThreadImpl in a TLS slot so it is impossible to access
// incorrectly from the wrong thread.
base::LazyInstance<base::ThreadLocalPointer<RenderThreadImpl>>::DestructorAtExit
    lazy_tls = LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<scoped_refptr<base::SingleThreadTaskRunner>>::
    DestructorAtExit g_main_task_runner = LAZY_INSTANCE_INITIALIZER;

// v8::MemoryPressureLevel should correspond to base::MemoryPressureListener.
static_assert(static_cast<v8::MemoryPressureLevel>(
                  base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE) ==
                  v8::MemoryPressureLevel::kNone,
              "none level not align");
static_assert(
    static_cast<v8::MemoryPressureLevel>(
        base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE) ==
        v8::MemoryPressureLevel::kModerate,
    "moderate level not align");
static_assert(
    static_cast<v8::MemoryPressureLevel>(
        base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) ==
        v8::MemoryPressureLevel::kCritical,
    "critical level not align");

// WebMemoryPressureLevel should correspond to base::MemoryPressureListener.
static_assert(static_cast<blink::WebMemoryPressureLevel>(
                  base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE) ==
                  blink::kWebMemoryPressureLevelNone,
              "blink::WebMemoryPressureLevelNone not align");
static_assert(
    static_cast<blink::WebMemoryPressureLevel>(
        base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE) ==
        blink::kWebMemoryPressureLevelModerate,
    "blink::WebMemoryPressureLevelModerate not align");
static_assert(
    static_cast<blink::WebMemoryPressureLevel>(
        base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) ==
        blink::kWebMemoryPressureLevelCritical,
    "blink::WebMemoryPressureLevelCritical not align");

void* CreateHistogram(const char* name, int min, int max, size_t buckets) {
  if (min <= 0)
    min = 1;
  std::string histogram_name;
  RenderThreadImpl* render_thread_impl = RenderThreadImpl::current();
  if (render_thread_impl) {  // Can be null in tests.
    histogram_name = render_thread_impl->histogram_customizer()
                         ->ConvertToCustomHistogramName(name);
  } else {
    histogram_name = std::string(name);
  }
  base::HistogramBase* histogram =
      base::Histogram::FactoryGet(histogram_name, min, max, buckets,
                                  base::Histogram::kUmaTargetedHistogramFlag);
  return histogram;
}

void AddHistogramSample(void* hist, int sample) {
  base::Histogram* histogram = static_cast<base::Histogram*>(hist);
  histogram->Add(sample);
}

void AddCrashKey(v8::CrashKeyId id, const std::string& value) {
  namespace bd = base::debug;
  switch (id) {
    case v8::CrashKeyId::kIsolateAddress:
      static bd::CrashKeyString* isolate_address = bd::AllocateCrashKeyString(
          "v8_isolate_address", bd::CrashKeySize::Size32);
      bd::SetCrashKeyString(isolate_address, value);
      break;
    case v8::CrashKeyId::kReadonlySpaceFirstPageAddress:
      static bd::CrashKeyString* ro_space_firstpage_address =
          bd::AllocateCrashKeyString("v8_ro_space_firstpage_address",
                                     bd::CrashKeySize::Size32);
      bd::SetCrashKeyString(ro_space_firstpage_address, value);
      break;
    case v8::CrashKeyId::kMapSpaceFirstPageAddress:
      static bd::CrashKeyString* map_space_firstpage_address =
          bd::AllocateCrashKeyString("v8_map_space_firstpage_address",
                                     bd::CrashKeySize::Size32);
      bd::SetCrashKeyString(map_space_firstpage_address, value);
      break;
    case v8::CrashKeyId::kCodeSpaceFirstPageAddress:
      static bd::CrashKeyString* code_space_firstpage_address =
          bd::AllocateCrashKeyString("v8_code_space_firstpage_address",
                                     bd::CrashKeySize::Size32);
      bd::SetCrashKeyString(code_space_firstpage_address, value);
      break;
    case v8::CrashKeyId::kDumpType:
      static bd::CrashKeyString* dump_type =
          bd::AllocateCrashKeyString("dump-type", bd::CrashKeySize::Size32);
      bd::SetCrashKeyString(dump_type, value);
      break;
    default:
      // Doing nothing for new keys is a valid option. Having this case allows
      // to introduce new CrashKeyId's without triggering a build break.
      break;
  }
}

scoped_refptr<viz::ContextProviderCommandBuffer> CreateOffscreenContext(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    const gpu::SharedMemoryLimits& limits,
    bool support_locking,
    bool support_gles2_interface,
    bool support_raster_interface,
    bool support_oop_rasterization,
    bool support_grcontext,
    bool automatic_flushes,
    viz::command_buffer_metrics::ContextType type,
    int32_t stream_id,
    gpu::SchedulingPriority stream_priority) {
  DCHECK(gpu_channel_host);
  // This is used to create a few different offscreen contexts:
  // - The shared main thread context, used by blink for 2D Canvas.
  // - The compositor worker context, used for GPU raster.
  // - The media context, used for accelerated video decoding.
  // This is for an offscreen context, so the default framebuffer doesn't need
  // alpha, depth, stencil, antialiasing.
  gpu::ContextCreationAttribs attributes;
  attributes.alpha_size = -1;
  attributes.depth_size = 0;
  attributes.stencil_size = 0;
  attributes.samples = 0;
  attributes.sample_buffers = 0;
  attributes.bind_generates_resource = false;
  attributes.lose_context_when_out_of_memory = true;
  attributes.enable_gles2_interface = support_gles2_interface;
  attributes.enable_raster_interface = support_raster_interface;
  attributes.enable_grcontext = support_grcontext;
  // Using RasterDecoder for OOP-R backend, so we need support_raster_interface
  // and !support_gles2_interface.
  attributes.enable_oop_rasterization = support_oop_rasterization &&
                                        support_raster_interface &&
                                        !support_gles2_interface;
  return base::MakeRefCounted<viz::ContextProviderCommandBuffer>(
      std::move(gpu_channel_host), gpu_memory_buffer_manager, stream_id,
      stream_priority, gpu::kNullSurfaceHandle,
      GURL("chrome://gpu/RenderThreadImpl::CreateOffscreenContext/" +
           viz::command_buffer_metrics::ContextTypeToString(type)),
      automatic_flushes, support_locking, support_grcontext, limits, attributes,
      type);
}

// Hook that allows single-sample metric code from //components/metrics to
// connect from the renderer process to the browser process.
void CreateSingleSampleMetricsProvider(
    mojo::SharedRemote<mojom::ChildProcessHost> process_host,
    mojo::PendingReceiver<metrics::mojom::SingleSampleMetricsProvider>
        receiver) {
  process_host->BindHostReceiver(std::move(receiver));
}

// This factory is used to defer binding of the InterfacePtr to the compositor
// thread.
class UkmRecorderFactoryImpl : public cc::UkmRecorderFactory {
 public:
  explicit UkmRecorderFactoryImpl(
      mojo::SharedRemote<mojom::ChildProcessHost> process_host)
      : process_host_(std::move(process_host)) {}
  ~UkmRecorderFactoryImpl() override = default;

  std::unique_ptr<ukm::UkmRecorder> CreateRecorder() override {
    mojo::PendingRemote<ukm::mojom::UkmRecorderInterface> recorder;
    process_host_->BindHostReceiver(recorder.InitWithNewPipeAndPassReceiver());
    return std::make_unique<ukm::MojoUkmRecorder>(std::move(recorder));
  }

 private:
  const mojo::SharedRemote<mojom::ChildProcessHost> process_host_;
};

}  // namespace

RenderThreadImpl::HistogramCustomizer::HistogramCustomizer() {
  custom_histograms_.insert("V8.MemoryExternalFragmentationTotal");
  custom_histograms_.insert("V8.MemoryHeapSampleTotalCommitted");
  custom_histograms_.insert("V8.MemoryHeapSampleTotalUsed");
  custom_histograms_.insert("V8.MemoryHeapUsed");
  custom_histograms_.insert("V8.MemoryHeapCommitted");
}

RenderThreadImpl::HistogramCustomizer::~HistogramCustomizer() {}

void RenderThreadImpl::HistogramCustomizer::RenderViewNavigatedToHost(
    const std::string& host,
    size_t view_count) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableHistogramCustomizer)) {
    return;
  }
  // Check if all RenderViews are displaying a page from the same host. If there
  // is only one RenderView, the common host is this view's host. If there are
  // many, check if this one shares the common host of the other
  // RenderViews. It's ok to not detect some cases where the RenderViews share a
  // common host. This information is only used for producing custom histograms.
  if (view_count == 1)
    SetCommonHost(host);
  else if (host != common_host_)
    SetCommonHost(std::string());
}

std::string RenderThreadImpl::HistogramCustomizer::ConvertToCustomHistogramName(
    const char* histogram_name) const {
  std::string name(histogram_name);
  if (!common_host_histogram_suffix_.empty() &&
      custom_histograms_.find(name) != custom_histograms_.end())
    name += common_host_histogram_suffix_;
  return name;
}

void RenderThreadImpl::HistogramCustomizer::SetCommonHost(
    const std::string& host) {
  if (host != common_host_) {
    common_host_ = host;
    common_host_histogram_suffix_ = HostToCustomHistogramSuffix(host);
    blink::MainThreadIsolate()->SetCreateHistogramFunction(CreateHistogram);
  }
}

std::string RenderThreadImpl::HistogramCustomizer::HostToCustomHistogramSuffix(
    const std::string& host) {
  if (host == "mail.google.com")
    return ".gmail";
  if (host == "docs.google.com" || host == "drive.google.com")
    return ".docs";
  if (host == "plus.google.com")
    return ".plus";
  if (host == "inbox.google.com")
    return ".inbox";
  if (host == "calendar.google.com")
    return ".calendar";
  if (host == "www.youtube.com")
    return ".youtube";
  if (IsAlexaTop10NonGoogleSite(host))
    return ".top10";

  return std::string();
}

bool RenderThreadImpl::HistogramCustomizer::IsAlexaTop10NonGoogleSite(
    const std::string& host) {
  // The Top10 sites have different TLD and/or subdomains depending on the
  // localization.
  if (host == "sina.com.cn")
    return true;

  std::string sanitized_host =
      net::registry_controlled_domains::GetDomainAndRegistry(
          host, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

  if (sanitized_host == "facebook.com")
    return true;
  if (sanitized_host == "baidu.com")
    return true;
  if (sanitized_host == "qq.com")
    return true;
  if (sanitized_host == "twitter.com")
    return true;
  if (sanitized_host == "taobao.com")
    return true;
  if (sanitized_host == "live.com")
    return true;

  if (!sanitized_host.empty()) {
    std::vector<base::StringPiece> host_tokens = base::SplitStringPiece(
        sanitized_host, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

    if (host_tokens.size() >= 2) {
      if ((host_tokens[0] == "yahoo") || (host_tokens[0] == "amazon") ||
          (host_tokens[0] == "wikipedia")) {
        return true;
      }
    }
  }
  return false;
}

// static
RenderThreadImpl* RenderThreadImpl::current() {
  return lazy_tls.Pointer()->Get();
}

// static
mojom::RenderMessageFilter* RenderThreadImpl::current_render_message_filter() {
  if (g_render_message_filter_for_testing)
    return g_render_message_filter_for_testing;
  DCHECK(current());
  return current()->render_message_filter();
}

// static
RendererBlinkPlatformImpl* RenderThreadImpl::current_blink_platform_impl() {
  if (g_current_blink_platform_impl_for_testing)
    return g_current_blink_platform_impl_for_testing;
  DCHECK(current());
  return current()->blink_platform_impl();
}

// static
void RenderThreadImpl::SetRenderMessageFilterForTesting(
    mojom::RenderMessageFilter* render_message_filter) {
  g_render_message_filter_for_testing = render_message_filter;
}

// static
void RenderThreadImpl::SetRendererBlinkPlatformImplForTesting(
    RendererBlinkPlatformImpl* blink_platform_impl) {
  g_current_blink_platform_impl_for_testing = blink_platform_impl;
}

// static
scoped_refptr<base::SingleThreadTaskRunner>
RenderThreadImpl::DeprecatedGetMainTaskRunner() {
  return g_main_task_runner.Get();
}

// In single-process mode used for debugging, we don't pass a renderer client
// ID via command line because RenderThreadImpl lives in the same process as
// the browser
RenderThreadImpl::RenderThreadImpl(
    const InProcessChildThreadParams& params,
    int32_t client_id,
    std::unique_ptr<blink::scheduler::WebThreadScheduler> scheduler)
    : ChildThreadImpl(
          base::DoNothing(),
          Options::Builder()
              .InBrowserProcess(params)
              .ConnectToBrowser(true)
              .IPCTaskRunner(scheduler->DeprecatedDefaultTaskRunner())
              .ExposesInterfacesToBrowser()
              .Build()),
      main_thread_scheduler_(std::move(scheduler)),
      categorized_worker_pool_(new CategorizedWorkerPool()),
      client_id_(client_id) {
  TRACE_EVENT0("startup", "RenderThreadImpl::Create");
  Init();
}

namespace {
int32_t GetClientIdFromCommandLine() {
  DCHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kRendererClientId));
  int32_t client_id;
  base::StringToInt(base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                        switches::kRendererClientId),
                    &client_id);
  return client_id;
}
}  // anonymous namespace

// Multi-process mode.
RenderThreadImpl::RenderThreadImpl(
    base::RepeatingClosure quit_closure,
    std::unique_ptr<blink::scheduler::WebThreadScheduler> scheduler)
    : ChildThreadImpl(
          std::move(quit_closure),
          Options::Builder()
              .ConnectToBrowser(true)
              .IPCTaskRunner(scheduler->DeprecatedDefaultTaskRunner())
              .ExposesInterfacesToBrowser()
              .Build()),
      main_thread_scheduler_(std::move(scheduler)),
      categorized_worker_pool_(new CategorizedWorkerPool()),
      client_id_(GetClientIdFromCommandLine()) {
  TRACE_EVENT0("startup", "RenderThreadImpl::Create");
  Init();
}

void RenderThreadImpl::Init() {
  TRACE_EVENT0("startup", "RenderThreadImpl::Init");

  GetContentClient()->renderer()->PostIOThreadCreated(GetIOTaskRunner().get());

  base::trace_event::TraceLog::GetInstance()->SetThreadSortIndex(
      base::PlatformThread::CurrentId(),
      kTraceEventRendererMainThreadSortIndex);

#if BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
  // On Mac and Android Java UI, the select popups are rendered by the browser.
#if defined(OS_MAC)
  // When UseCommonSelectPopup is enabled, the internal popup menu should be
  // used.
  if (!features::IsUseCommonSelectPopupEnabled())
#endif
    blink::WebView::SetUseExternalPopupMenus(true);
#endif

  lazy_tls.Pointer()->Set(this);
  g_main_task_runner.Get() = base::ThreadTaskRunnerHandle::Get();

  // Register this object as the main thread.
  ChildProcess::current()->set_main_thread(this);

  metrics::InitializeSingleSampleMetricsFactory(base::BindRepeating(
      &CreateSingleSampleMetricsProvider, child_process_host()));

  mojo::PendingRemote<viz::mojom::Gpu> remote_gpu;
  BindHostReceiver(remote_gpu.InitWithNewPipeAndPassReceiver());
  gpu_ = viz::Gpu::Create(std::move(remote_gpu), GetIOTaskRunner());

  resource_dispatcher_.reset(new ResourceDispatcher());

  // NOTE: Do not add interfaces to |binders| within this method. Instead,
  // modify the definition of |ExposeRendererInterfacesToBrowser()| to ensure
  // security review coverage.
  mojo::BinderMap binders;
  InitializeWebKit(&binders);

  vc_manager_.reset(new blink::WebVideoCaptureImplManager());

  unfreezable_message_filter_ = new UnfreezableMessageFilter(this);
  AddFilter(unfreezable_message_filter_.get());

  GetContentClient()->renderer()->RenderThreadStarted();
  ExposeRendererInterfacesToBrowser(weak_factory_.GetWeakPtr(), &binders);
  ExposeInterfacesToBrowser(std::move(binders));

  url_loader_throttle_provider_ =
      GetContentClient()->renderer()->CreateURLLoaderThrottleProvider(
          URLLoaderThrottleProviderType::kFrame);

  GetAssociatedInterfaceRegistry()->AddInterface(base::BindRepeating(
      &RenderThreadImpl::OnRouteProviderReceiver, base::Unretained(this)));
  GetAssociatedInterfaceRegistry()->AddInterface(base::BindRepeating(
      &RenderThreadImpl::OnRendererInterfaceReceiver, base::Unretained(this)));

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

#if defined(ENABLE_IPC_FUZZER)
  if (command_line.HasSwitch(switches::kIpcDumpDirectory)) {
    base::FilePath dump_directory =
        command_line.GetSwitchValuePath(switches::kIpcDumpDirectory);
    IPC::ChannelProxy::OutgoingMessageFilter* filter =
        LoadExternalIPCDumper(dump_directory);
    GetChannel()->set_outgoing_message_filter(filter);
    mojo::MessageDumper::SetMessageDumpDirectory(dump_directory);
  }
#endif

  cc::SetClientNameForMetrics("Renderer");

  is_threaded_animation_enabled_ =
      !command_line.HasSwitch(cc::switches::kDisableThreadedAnimation);

// On macOS this value is adjusted in `UpdateScrollbarTheme()`,
// but the system default is true.
#if defined(OS_MAC)
  is_elastic_overscroll_enabled_ = true;
#elif defined(OS_WIN)
  is_elastic_overscroll_enabled_ =
      base::FeatureList::IsEnabled(features::kElasticOverscrollWin);
#else
  is_elastic_overscroll_enabled_ = false;
#endif

  is_zoom_for_dsf_enabled_ = content::IsUseZoomForDSFEnabled();

  if (command_line.HasSwitch(switches::kDisableLCDText)) {
    is_lcd_text_enabled_ = false;
  } else if (command_line.HasSwitch(switches::kEnableLCDText)) {
    is_lcd_text_enabled_ = true;
  } else {
#if defined(OS_ANDROID)
    is_lcd_text_enabled_ = false;
#elif defined(OS_MAC)
    if (base::FeatureList::IsEnabled(features::kRespectMacLCDTextSetting))
      is_lcd_text_enabled_ = IsSubpixelAntialiasingAvailable();
    else
      is_lcd_text_enabled_ = true;
#else
    is_lcd_text_enabled_ = true;
#endif
  }

  if (command_line.HasSwitch(switches::kDisableGpuCompositing))
    is_gpu_compositing_disabled_ = true;

  // Note that under Linux, the media library will normally already have
  // been initialized by the Zygote before this instance became a Renderer.
  media::InitializeMediaLibrary();

#if defined(OS_ANDROID)
  if (!command_line.HasSwitch(switches::kDisableAcceleratedVideoDecode) &&
      media::MediaCodecUtil::IsMediaCodecAvailable()) {
    media::EnablePlatformDecoderSupport();
  }
#endif

  memory_pressure_listener_ = std::make_unique<base::MemoryPressureListener>(
      FROM_HERE,
      base::BindRepeating(&RenderThreadImpl::OnMemoryPressure,
                          base::Unretained(this)),
      base::BindRepeating(&RenderThreadImpl::OnSyncMemoryPressure,
                          base::Unretained(this)));

  int num_raster_threads = 0;
  std::string string_value =
      command_line.GetSwitchValueASCII(switches::kNumRasterThreads);
  bool parsed_num_raster_threads =
      base::StringToInt(string_value, &num_raster_threads);
  DCHECK(parsed_num_raster_threads) << string_value;
  DCHECK_GT(num_raster_threads, 0);

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  categorized_worker_pool_->SetBackgroundingCallback(
      main_thread_scheduler_->DefaultTaskRunner(),
      base::BindOnce(
          [](base::WeakPtr<RenderThreadImpl> render_thread,
             base::PlatformThreadId thread_id) {
            if (!render_thread)
              return;
            render_thread->render_message_filter()->SetThreadPriority(
                thread_id, base::ThreadPriority::BACKGROUND);
          },
          weak_factory_.GetWeakPtr()));
#endif
  categorized_worker_pool_->Start(num_raster_threads);

  discardable_memory_allocator_ = CreateDiscardableMemoryAllocator();

  // TODO(boliu): In single process, browser main loop should set up the
  // discardable memory manager, and should skip this if kSingleProcess.
  // See crbug.com/503724.
  base::DiscardableMemoryAllocator::SetInstance(
      discardable_memory_allocator_.get());

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  if (base::FeatureList::IsEnabled(
          blink::features::kBlinkCompositorUseDisplayThreadPriority)) {
    render_message_filter()->SetThreadPriority(
        ChildProcess::current()->io_thread_id(), base::ThreadPriority::DISPLAY);
  }
#endif

  process_foregrounded_count_ = 0;
  needs_to_record_first_active_paint_ = false;
  was_backgrounded_time_ = base::TimeTicks::Min();

  BindHostReceiver(frame_sink_provider_.BindNewPipeAndPassReceiver());

  if (!is_gpu_compositing_disabled_) {
    BindHostReceiver(compositing_mode_reporter_.BindNewPipeAndPassReceiver());

    compositing_mode_reporter_->AddCompositingModeWatcher(
        compositing_mode_watcher_receiver_.BindNewPipeAndPassRemote());
  }

  variations_observer_ = std::make_unique<VariationsRenderThreadObserver>();
  AddObserver(variations_observer_.get());
}

RenderThreadImpl::~RenderThreadImpl() {
  g_main_task_runner.Get() = nullptr;

  // Need to make sure this reference is removed on the correct task runner;
  if (video_frame_compositor_task_runner_ &&
      video_frame_compositor_context_provider_) {
    video_frame_compositor_task_runner_->ReleaseSoon(
        FROM_HERE, std::move(video_frame_compositor_context_provider_));
  }
}

void RenderThreadImpl::Shutdown() {
  ChildThreadImpl::Shutdown();
  // In a multi-process mode, we immediately exit the renderer.
  // Historically we had a graceful shutdown sequence here but it was
  // 1) a waste of performance and 2) a source of lots of complicated
  // crashes caused by shutdown ordering. Immediate exit eliminates
  // those problems.

  // Give the V8 isolate a chance to dump internal stats useful for performance
  // evaluation and debugging.
  blink::MainThreadIsolate()->DumpAndResetStats();

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDumpBlinkRuntimeCallStats))
    blink::LogRuntimeCallStats();

  // In a single-process mode, we cannot call _exit(0) in Shutdown() because
  // it will exit the process before the browser side is ready to exit.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess))
    base::Process::TerminateCurrentProcessImmediately(0);
}

bool RenderThreadImpl::ShouldBeDestroyed() {
  DCHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSingleProcess));
  // In a single-process mode, it is unsafe to destruct this renderer thread
  // because we haven't run the shutdown sequence. Hence we leak the render
  // thread.
  //
  // In this case, we also need to disable at-exit callbacks because some of
  // the at-exit callbacks are expected to run after the renderer thread
  // has been destructed.
  base::AtExitManager::DisableAllAtExitManagers();
  return false;
}

bool RenderThreadImpl::Send(IPC::Message* msg) {
  // There are cases where we want to pump asynchronous messages while waiting
  // synchronously for the replies to the message to be sent here. However, this
  // may create an opportunity for re-entrancy into WebKit and other subsystems,
  // so we need to take care to disable callbacks, timers, and pending network
  // loads that could trigger such callbacks.
  bool pumping_events = false;
  if (msg->is_sync()) {
    if (msg->is_caller_pumping_messages()) {
      pumping_events = true;
    }
  }

  std::unique_ptr<blink::scheduler::WebThreadScheduler::RendererPauseHandle>
      renderer_paused_handle;
  std::unique_ptr<blink::WebScopedPagePauser> page_pauser_handle;

  if (pumping_events) {
    renderer_paused_handle = main_thread_scheduler_->PauseRenderer();
    page_pauser_handle = blink::WebScopedPagePauser::Create();
  }

  return ChildThreadImpl::Send(msg);
}

IPC::SyncChannel* RenderThreadImpl::GetChannel() {
  return channel();
}

std::string RenderThreadImpl::GetLocale() {
  // The browser process should have passed the locale to the renderer via the
  // --lang command line flag.
  const base::CommandLine& parsed_command_line =
      *base::CommandLine::ForCurrentProcess();
  const std::string& lang =
      parsed_command_line.GetSwitchValueASCII(switches::kLang);
  DCHECK(!lang.empty());
  return lang;
}

IPC::SyncMessageFilter* RenderThreadImpl::GetSyncMessageFilter() {
  return sync_message_filter();
}

void RenderThreadImpl::AddRoute(int32_t routing_id, IPC::Listener* listener) {
  ChildThreadImpl::GetRouter()->AddRoute(routing_id, listener);
  auto it = pending_frames_.find(routing_id);
  if (it == pending_frames_.end())
    return;

  RenderFrameImpl* frame = RenderFrameImpl::FromRoutingID(routing_id);
  if (!frame)
    return;

  GetChannel()->AddListenerTaskRunner(
      routing_id,
      frame->GetTaskRunner(blink::TaskType::kInternalNavigationAssociated));

  unfreezable_message_filter_->AddListenerUnfreezableTaskRunner(
      routing_id,
      frame->GetTaskRunner(
          blink::TaskType::kInternalNavigationAssociatedUnfreezable));

  frame->BindFrame(std::move(it->second));
  pending_frames_.erase(it);
}

void RenderThreadImpl::RemoveRoute(int32_t routing_id) {
  ChildThreadImpl::GetRouter()->RemoveRoute(routing_id);
  unfreezable_message_filter_->RemoveListenerUnfreezableTaskRunner(routing_id);
  GetChannel()->RemoveListenerTaskRunner(routing_id);
  pending_frames_.erase(routing_id);
}

void RenderThreadImpl::RegisterPendingFrameCreate(
    int routing_id,
    mojo::PendingReceiver<mojom::Frame> frame_receiver) {
  auto pair = pending_frames_.emplace(routing_id, std::move(frame_receiver));
  CHECK(pair.second) << "Inserting a duplicate item.";
}

mojom::RendererHost* RenderThreadImpl::GetRendererHost() {
  if (!renderer_host_) {
    DCHECK(GetChannel());
    GetChannel()->GetRemoteAssociatedInterface(&renderer_host_);
  }
  return renderer_host_.get();
}

int RenderThreadImpl::GenerateRoutingID() {
  int32_t routing_id = MSG_ROUTING_NONE;
  render_message_filter()->GenerateRoutingID(&routing_id);
  return routing_id;
}

void RenderThreadImpl::AddFilter(IPC::MessageFilter* filter) {
  channel()->AddFilter(filter);
}

void RenderThreadImpl::RemoveFilter(IPC::MessageFilter* filter) {
  channel()->RemoveFilter(filter);
}

void RenderThreadImpl::AddObserver(RenderThreadObserver* observer) {
  observers_.AddObserver(observer);
  observer->RegisterMojoInterfaces(&associated_interfaces_);
}

void RenderThreadImpl::RemoveObserver(RenderThreadObserver* observer) {
  observer->UnregisterMojoInterfaces(&associated_interfaces_);
  observers_.RemoveObserver(observer);
}

void RenderThreadImpl::SetResourceDispatcherDelegate(
    ResourceDispatcherDelegate* delegate) {
  resource_dispatcher_->set_delegate(delegate);
}

void RenderThreadImpl::InitializeCompositorThread() {
  blink_platform_impl_->CreateAndSetCompositorThread();
  compositor_task_runner_ = blink_platform_impl_->CompositorThreadTaskRunner();
  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&ThreadRestrictions::SetIOAllowed),
                     false));
  GetContentClient()->renderer()->PostCompositorThreadCreated(
      compositor_task_runner_.get());
}

scoped_refptr<base::SingleThreadTaskRunner>
RenderThreadImpl::CreateVideoFrameCompositorTaskRunner() {
  if (!video_frame_compositor_task_runner_) {
    // All of Chromium's GPU code must know which thread it's running on, and
    // be the same thread on which the rendering context was initialized. This
    // is why this must be a SingleThreadTaskRunner instead of a
    // SequencedTaskRunner.
    video_frame_compositor_task_runner_ =
        base::ThreadPool::CreateSingleThreadTaskRunner(
            {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
  }

  return video_frame_compositor_task_runner_;
}

mojom::RouteProvider* RenderThreadImpl::GetRemoteRouteProvider(
    util::PassKey<AgentSchedulingGroup>) {
  if (!remote_route_provider_) {
    DCHECK(GetChannel());
    GetChannel()->GetRemoteAssociatedInterface(&remote_route_provider_);
  }

  return remote_route_provider_.get();
}

void RenderThreadImpl::InitializeWebKit(mojo::BinderMap* binders) {
  DCHECK(!blink_platform_impl_);

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

#ifdef ENABLE_VTUNE_JIT_INTERFACE
  if (command_line.HasSwitch(switches::kEnableVtune))
    gin::Debug::SetJitCodeEventHandler(vTune::GetVtuneCodeEventHandler());
#endif

  blink_platform_impl_.reset(
      new RendererBlinkPlatformImpl(main_thread_scheduler_.get()));
  // This, among other things, enables any feature marked "test" in
  // runtime_enabled_features. It is run before
  // SetRuntimeFeaturesDefaultsAndUpdateFromArgs() so that command line
  // arguments take precedence over (and can disable) "test" features.
  GetContentClient()
      ->renderer()
      ->SetRuntimeFeaturesDefaultsBeforeBlinkInitialization();
  SetRuntimeFeaturesDefaultsAndUpdateFromArgs(command_line);

  blink::Initialize(blink_platform_impl_.get(), binders,
                    main_thread_scheduler_.get());

  v8::Isolate* isolate = blink::MainThreadIsolate();
  isolate->SetCreateHistogramFunction(CreateHistogram);
  isolate->SetAddHistogramSampleFunction(AddHistogramSample);
  isolate->SetAddCrashKeyCallback(AddCrashKey);

  main_thread_compositor_task_runner_ =
      main_thread_scheduler_->CompositorTaskRunner();

  if (!command_line.HasSwitch(switches::kDisableThreadedCompositing))
    InitializeCompositorThread();

  RenderThreadImpl::RegisterSchemes();

  RenderMediaClient::Initialize();

  if (!GetContentClient()->renderer()->RunIdleHandlerWhenWidgetsHidden()) {
    // If we do not track widget visibility, then assume conservatively that
    // the isolate is in background. This reduces memory usage.
    isolate->IsolateInBackgroundNotification();
  }

  // Hook up blink's codecs so skia can call them. Since only the renderer
  // processes should be doing image decoding, this is not done in the common
  // skia initialization code for the GPU.
  SkGraphics::SetImageGeneratorFromEncodedDataFactory(
      blink::WebImageGenerator::CreateAsSkImageGenerator);

  if (command_line.HasSwitch(network::switches::kExplicitlyAllowedPorts)) {
    std::string allowed_ports = command_line.GetSwitchValueASCII(
        network::switches::kExplicitlyAllowedPorts);
    net::SetExplicitlyAllowedPorts(allowed_ports);
  }
}

void RenderThreadImpl::RegisterSchemes() {
  // chrome:
  WebString chrome_scheme(WebString::FromASCII(kChromeUIScheme));
  WebSecurityPolicy::RegisterURLSchemeAsDisplayIsolated(chrome_scheme);
  WebSecurityPolicy::RegisterURLSchemeAsNotAllowingJavascriptURLs(
      chrome_scheme);

  // chrome-untrusted:
  WebString chrome_untrusted_scheme(
      WebString::FromASCII(kChromeUIUntrustedScheme));
  WebSecurityPolicy::RegisterURLSchemeAsNotAllowingJavascriptURLs(
      chrome_untrusted_scheme);
  WebSecurityPolicy::RegisterURLSchemeAsSupportingFetchAPI(
      chrome_untrusted_scheme);

  // devtools:
  WebString devtools_scheme(WebString::FromASCII(kChromeDevToolsScheme));
  WebSecurityPolicy::RegisterURLSchemeAsDisplayIsolated(devtools_scheme);

  // view-source:
  WebString view_source_scheme(WebString::FromASCII(kViewSourceScheme));
  WebSecurityPolicy::RegisterURLSchemeAsDisplayIsolated(view_source_scheme);

  // chrome-error:
  WebString error_scheme(WebString::FromASCII(kChromeErrorScheme));
  WebSecurityPolicy::RegisterURLSchemeAsDisplayIsolated(error_scheme);
  WebSecurityPolicy::RegisterURLSchemeAsNotAllowingJavascriptURLs(error_scheme);

  // googlechrome:
  WebString google_chrome_scheme(WebString::FromASCII(kGoogleChromeScheme));
  WebSecurityPolicy::RegisterURLSchemeAsDisplayIsolated(google_chrome_scheme);
}

void RenderThreadImpl::RecordAction(const base::UserMetricsAction& action) {
  GetRendererHost()->RecordUserMetricsAction(action.str_);
}

void RenderThreadImpl::RecordComputedAction(const std::string& action) {
  GetRendererHost()->RecordUserMetricsAction(action);
}

void RenderThreadImpl::RegisterExtension(
    std::unique_ptr<v8::Extension> extension) {
  WebScriptController::RegisterExtension(std::move(extension));
}

int RenderThreadImpl::PostTaskToAllWebWorkers(base::RepeatingClosure closure) {
  return WorkerThreadRegistry::Instance()->PostTaskToAllThreads(
      std::move(closure));
}

bool RenderThreadImpl::ResolveProxy(const GURL& url, std::string* proxy_list) {
  base::Optional<std::string> result;
  GetRendererHost()->ResolveProxy(url, &result);
  *proxy_list = result.value_or(std::string());
  return result.has_value();
}

media::GpuVideoAcceleratorFactories* RenderThreadImpl::GetGpuFactories() {
  DCHECK(IsMainThread());

  if (!gpu_factories_.empty()) {
    if (!gpu_factories_.back()->CheckContextProviderLostOnMainThread())
      return gpu_factories_.back().get();

    GetMediaThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&GpuVideoAcceleratorFactoriesImpl::DestroyContext,
                       base::Unretained(gpu_factories_.back().get())));
  }

  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();

  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host =
      EstablishGpuChannelSync();
  if (!gpu_channel_host)
    return nullptr;
  // Currently, VideoResourceUpdater can't convert hardware resources to
  // software resources in software compositing mode.  So, fall back to software
  // video decoding if gpu compositing is off.
  if (is_gpu_compositing_disabled_)
    return nullptr;
  // This context is only used to create textures and mailbox them, so
  // use lower limits than the default.
  gpu::SharedMemoryLimits limits = gpu::SharedMemoryLimits::ForMailboxContext();
  bool support_locking = false;
  bool support_gles2_interface = true;
  bool support_raster_interface = false;
  bool support_oop_rasterization = false;
  bool support_grcontext = false;
  bool automatic_flushes = false;
  scoped_refptr<viz::ContextProviderCommandBuffer> media_context_provider =
      CreateOffscreenContext(
          gpu_channel_host, GetGpuMemoryBufferManager(), limits,
          support_locking, support_gles2_interface, support_raster_interface,
          support_oop_rasterization, support_grcontext, automatic_flushes,
          viz::command_buffer_metrics::ContextType::MEDIA, kGpuStreamIdMedia,
          kGpuStreamPriorityMedia);

  const bool enable_video_accelerator =
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
      cmd_line->HasSwitch(switches::kEnableAcceleratedVideoDecode) &&
#else
      !cmd_line->HasSwitch(switches::kDisableAcceleratedVideoDecode) &&
#endif  // defined(OS_LINUX) && !defined(OS_CHROMEOS)
      (gpu_channel_host->gpu_feature_info()
           .status_values[gpu::GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE] ==
       gpu::kGpuFeatureStatusEnabled);
  const bool enable_gpu_memory_buffers =
      !is_gpu_compositing_disabled_ &&
#if !defined(OS_ANDROID)
      !cmd_line->HasSwitch(switches::kDisableGpuMemoryBufferVideoFrames);
#else
      cmd_line->HasSwitch(switches::kEnableGpuMemoryBufferVideoFrames);
#endif  // defined(OS_MAC) || defined(OS_LINUX) || defined(OS_CHROMEOS) ||
        // defined(OS_WIN)
  const bool enable_media_stream_gpu_memory_buffers =
      enable_gpu_memory_buffers &&
      base::FeatureList::IsEnabled(
          features::kWebRtcUseGpuMemoryBufferVideoFrames);
  bool enable_video_gpu_memory_buffers = enable_gpu_memory_buffers;
#if defined(OS_WIN)
  enable_video_gpu_memory_buffers =
      enable_video_gpu_memory_buffers &&
      (cmd_line->HasSwitch(switches::kEnableGpuMemoryBufferVideoFrames) ||
       gpu_channel_host->gpu_info().overlay_info.supports_overlays);
#endif  // defined(OS_WIN)

  mojo::PendingRemote<media::mojom::InterfaceFactory> interface_factory;
  BindHostReceiver(interface_factory.InitWithNewPipeAndPassReceiver());

  mojo::PendingRemote<media::mojom::VideoEncodeAcceleratorProvider>
      vea_provider;
  gpu_->CreateVideoEncodeAcceleratorProvider(
      vea_provider.InitWithNewPipeAndPassReceiver());

  gpu_factories_.push_back(GpuVideoAcceleratorFactoriesImpl::Create(
      std::move(gpu_channel_host), base::ThreadTaskRunnerHandle::Get(),
      GetMediaThreadTaskRunner(), std::move(media_context_provider),
      enable_video_gpu_memory_buffers, enable_media_stream_gpu_memory_buffers,
      enable_video_accelerator, std::move(interface_factory),
      std::move(vea_provider)));
  gpu_factories_.back()->SetRenderingColorSpace(rendering_color_space_);
  return gpu_factories_.back().get();
}

scoped_refptr<viz::RasterContextProvider>
RenderThreadImpl::GetVideoFrameCompositorContextProvider(
    scoped_refptr<viz::RasterContextProvider> unwanted_context_provider) {
  DCHECK(video_frame_compositor_task_runner_);
  if (video_frame_compositor_context_provider_ &&
      video_frame_compositor_context_provider_ != unwanted_context_provider) {
    return video_frame_compositor_context_provider_;
  }

  // Need to make sure these references are removed on the correct task runner;
  if (video_frame_compositor_context_provider_) {
    video_frame_compositor_task_runner_->ReleaseSoon(
        FROM_HERE, std::move(video_frame_compositor_context_provider_));
  }

  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host =
      EstablishGpuChannelSync();
  if (!gpu_channel_host)
    return nullptr;

  // This context is only used to create textures and mailbox them, so
  // use lower limits than the default.
  gpu::SharedMemoryLimits limits = gpu::SharedMemoryLimits::ForMailboxContext();

  bool support_locking = false;
  bool support_gles2_interface = true;
  bool support_raster_interface = true;
  bool support_oop_rasterization = false;
  bool support_grcontext = false;
  bool automatic_flushes = false;
  video_frame_compositor_context_provider_ = CreateOffscreenContext(
      gpu_channel_host, GetGpuMemoryBufferManager(), limits, support_locking,
      support_gles2_interface, support_raster_interface,
      support_oop_rasterization, support_grcontext, automatic_flushes,
      viz::command_buffer_metrics::ContextType::RENDER_COMPOSITOR,
      kGpuStreamIdMedia, kGpuStreamPriorityMedia);
  return video_frame_compositor_context_provider_;
}

scoped_refptr<viz::ContextProviderCommandBuffer>
RenderThreadImpl::SharedMainThreadContextProvider() {
  DCHECK(IsMainThread());
  if (shared_main_thread_contexts_ &&
      shared_main_thread_contexts_->RasterInterface()
              ->GetGraphicsResetStatusKHR() == GL_NO_ERROR)
    return shared_main_thread_contexts_;

  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host(
      EstablishGpuChannelSync());
  if (!gpu_channel_host) {
    shared_main_thread_contexts_ = nullptr;
    return nullptr;
  }

  bool support_locking = false;
  bool support_raster_interface = true;
  bool support_oop_rasterization =
      base::FeatureList::IsEnabled(features::kCanvasOopRasterization);
  bool support_gles2_interface = false;
  bool support_grcontext = !support_oop_rasterization;
  // Enable automatic flushes to improve canvas throughput.
  // See https://crbug.com/880901
  bool automatic_flushes = true;
  shared_main_thread_contexts_ = CreateOffscreenContext(
      std::move(gpu_channel_host), GetGpuMemoryBufferManager(),
      gpu::SharedMemoryLimits(), support_locking, support_gles2_interface,
      support_raster_interface, support_oop_rasterization, support_grcontext,
      automatic_flushes,
      viz::command_buffer_metrics::ContextType::RENDERER_MAIN_THREAD,
      kGpuStreamIdDefault, kGpuStreamPriorityDefault);
  auto result = shared_main_thread_contexts_->BindToCurrentThread();
  if (result != gpu::ContextResult::kSuccess)
    shared_main_thread_contexts_ = nullptr;
  return shared_main_thread_contexts_;
}

#if defined(OS_ANDROID)

scoped_refptr<StreamTextureFactory> RenderThreadImpl::GetStreamTexureFactory() {
  DCHECK(IsMainThread());
  if (!stream_texture_factory_ || stream_texture_factory_->IsLost()) {
    scoped_refptr<gpu::GpuChannelHost> channel = EstablishGpuChannelSync();
    if (!channel) {
      stream_texture_factory_ = nullptr;
      return nullptr;
    }
    stream_texture_factory_ = StreamTextureFactory::Create(std::move(channel));
  }
  return stream_texture_factory_;
}

bool RenderThreadImpl::EnableStreamTextureCopy() {
  return GetContentClient()->UsingSynchronousCompositing();
}

#endif

base::WaitableEvent* RenderThreadImpl::GetShutdownEvent() {
  return ChildProcess::current()->GetShutDownEvent();
}

int32_t RenderThreadImpl::GetClientId() {
  return client_id_;
}

bool RenderThreadImpl::IsOnline() {
  return online_status_;
}

void RenderThreadImpl::SetRendererProcessType(
    blink::scheduler::WebRendererProcessType type) {
  main_thread_scheduler_->SetRendererProcessType(type);
}

blink::WebString RenderThreadImpl::GetUserAgent() {
  DCHECK(!user_agent_.IsNull());
  return user_agent_;
}

const blink::UserAgentMetadata& RenderThreadImpl::GetUserAgentMetadata() {
  return user_agent_metadata_;
}

bool RenderThreadImpl::IsUseZoomForDSF() {
  return IsUseZoomForDSFEnabled();
}

void RenderThreadImpl::OnAssociatedInterfaceRequest(
    const std::string& name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  if (!associated_interfaces_.TryBindInterface(name, &handle))
    ChildThreadImpl::OnAssociatedInterfaceRequest(name, std::move(handle));
}

scoped_refptr<base::SingleThreadTaskRunner>
RenderThreadImpl::GetIOTaskRunner() {
  return ChildProcess::current()->io_task_runner();
}

bool RenderThreadImpl::IsLcdTextEnabled() {
  return is_lcd_text_enabled_;
}

bool RenderThreadImpl::IsElasticOverscrollEnabled() {
  return is_elastic_overscroll_enabled_;
}

bool RenderThreadImpl::IsUseZoomForDSFEnabled() {
  return is_zoom_for_dsf_enabled_;
}

scoped_refptr<base::SingleThreadTaskRunner>
RenderThreadImpl::GetCompositorMainThreadTaskRunner() {
  return main_thread_compositor_task_runner_;
}

bool RenderThreadImpl::IsSingleThreaded() {
  return !compositor_task_runner_;
}

scoped_refptr<base::SingleThreadTaskRunner>
RenderThreadImpl::GetCleanupTaskRunner() {
  return current_blink_platform_impl()
      ->main_thread_scheduler()
      ->DefaultTaskRunner();
}

gpu::GpuMemoryBufferManager* RenderThreadImpl::GetGpuMemoryBufferManager() {
  return gpu_->gpu_memory_buffer_manager();
}

blink::scheduler::WebThreadScheduler*
RenderThreadImpl::GetWebMainThreadScheduler() {
  return main_thread_scheduler_.get();
}

std::unique_ptr<viz::SyntheticBeginFrameSource>
RenderThreadImpl::CreateSyntheticBeginFrameSource() {
  base::SingleThreadTaskRunner* compositor_impl_side_task_runner =
      compositor_task_runner_ ? compositor_task_runner_.get()
                              : base::ThreadTaskRunnerHandle::Get().get();
  return std::make_unique<viz::BackToBackBeginFrameSource>(
      std::make_unique<viz::DelayBasedTimeSource>(
          compositor_impl_side_task_runner));
}

cc::TaskGraphRunner* RenderThreadImpl::GetTaskGraphRunner() {
  return categorized_worker_pool_->GetTaskGraphRunner();
}

bool RenderThreadImpl::IsThreadedAnimationEnabled() {
  return is_threaded_animation_enabled_;
}

bool RenderThreadImpl::IsScrollAnimatorEnabled() {
  return is_scroll_animator_enabled_;
}

std::unique_ptr<cc::UkmRecorderFactory>
RenderThreadImpl::CreateUkmRecorderFactory() {
  return std::make_unique<UkmRecorderFactoryImpl>(child_process_host());
}

bool RenderThreadImpl::IsMainThread() {
  return !!current();
}

void RenderThreadImpl::OnChannelError() {
  // In single-process mode, the renderer can't be restarted after shutdown.
  // So, if we get a channel error, crash the whole process right now to get a
  // more informative stack, since we will otherwise just crash later when we
  // try to restart it.
  CHECK(!base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSingleProcess));
  ChildThreadImpl::OnChannelError();
}

void RenderThreadImpl::OnProcessFinalRelease() {
  // Do not shutdown the process. The browser process is the only one
  // responsible for renderer shutdown.
  //
  // Renderer process used to request self shutdown. It has been removed. It
  // caused race conditions, where the browser process was reusing renderer
  // processes that were shutting down.
  // See https://crbug.com/535246 or https://crbug.com/873541/#c8.
  NOTREACHED();
}

bool RenderThreadImpl::OnControlMessageReceived(const IPC::Message& msg) {
  for (auto& observer : observers_) {
    if (observer.OnControlMessageReceived(msg))
      return true;
  }

  return false;
}

void RenderThreadImpl::SetSchedulerKeepActive(bool keep_active) {
  main_thread_scheduler_->SetSchedulerKeepActive(keep_active);
}

void RenderThreadImpl::SetProcessState(
    mojom::RenderProcessBackgroundState background_state,
    mojom::RenderProcessVisibleState visible_state) {
  DCHECK(background_state_ != background_state ||
         visible_state_ != visible_state);

  if (background_state != background_state_) {
    if (background_state == mojom::RenderProcessBackgroundState::kForegrounded)
      OnRendererForegrounded();
    else
      OnRendererBackgrounded();
  }

  if (visible_state != visible_state_) {
    if (visible_state == mojom::RenderProcessVisibleState::kVisible)
      OnRendererVisible();
    else
      OnRendererHidden();
  }

  background_state_ = background_state;
  visible_state_ = visible_state;
}

void RenderThreadImpl::SetIsLockedToSite() {
  DCHECK(blink_platform_impl_);
  blink_platform_impl_->SetIsLockedToSite();
}

#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
void RenderThreadImpl::WriteClangProfilingProfile(
    WriteClangProfilingProfileCallback callback) {
  // This will write the profiling profile to the file that has been opened and
  // passed to this renderer by the browser.
  base::WriteClangProfilingProfile();
  std::move(callback).Run();
}
#endif

void RenderThreadImpl::SetIsCrossOriginIsolated(bool value) {
  blink::SetIsCrossOriginIsolated(value);
}

bool RenderThreadImpl::GetRendererMemoryMetrics(
    RendererMemoryMetrics* memory_metrics) const {
  DCHECK(memory_metrics);

  // Cache this result, as it can change while this code is running, and is used
  // as a divisor below.
  size_t render_view_count = RenderView::GetRenderViewCount();

  // If there are no render views it doesn't make sense to calculate metrics
  // right now.
  if (render_view_count == 0)
    return false;

  blink::WebMemoryStatistics blink_stats = blink::WebMemoryStatistics::Get();
  memory_metrics->partition_alloc_kb =
      blink_stats.partition_alloc_total_allocated_bytes / 1024;
  memory_metrics->blink_gc_kb =
      blink_stats.blink_gc_total_allocated_bytes / 1024;
  std::unique_ptr<base::ProcessMetrics> metric(
      base::ProcessMetrics::CreateCurrentProcessMetrics());
  size_t malloc_usage = metric->GetMallocUsage();
  memory_metrics->malloc_mb = malloc_usage / 1024 / 1024;

  size_t discardable_usage = discardable_memory_allocator_->GetBytesAllocated();
  memory_metrics->discardable_kb = discardable_usage / 1024;

  size_t v8_usage = 0;
  if (v8::Isolate* isolate = blink::MainThreadIsolate()) {
    v8::HeapStatistics v8_heap_statistics;
    isolate->GetHeapStatistics(&v8_heap_statistics);
    v8_usage = v8_heap_statistics.total_heap_size();
  }
  // TODO(tasak): Currently only memory usage of mainThreadIsolate() is
  // reported. We should collect memory usages of all isolates using
  // memory-infra.
  memory_metrics->v8_main_thread_isolate_mb = v8_usage / 1024 / 1024;
  size_t total_allocated = blink_stats.partition_alloc_total_allocated_bytes +
                           blink_stats.blink_gc_total_allocated_bytes +
                           malloc_usage + v8_usage + discardable_usage;
  memory_metrics->total_allocated_mb = total_allocated / 1024 / 1024;
  memory_metrics->non_discardable_total_allocated_mb =
      (total_allocated - discardable_usage) / 1024 / 1024;
  memory_metrics->total_allocated_per_render_view_mb =
      total_allocated / render_view_count / 1024 / 1024;

  return true;
}

static void RecordMemoryUsageAfterBackgroundedMB(const char* basename,
                                                 const char* suffix,
                                                 int memory_usage) {
  std::string histogram_name = base::StringPrintf("%s.%s", basename, suffix);
  base::UmaHistogramMemoryLargeMB(histogram_name, memory_usage);
}

void RenderThreadImpl::RecordMemoryUsageAfterBackgrounded(
    const char* suffix,
    int foregrounded_count) {
  // If this renderer is resumed, we should not update UMA.
  if (!RendererIsHidden())
    return;
  // If this renderer was not kept backgrounded for 5/10/15 minutes,
  // we should not record current memory usage.
  if (foregrounded_count != process_foregrounded_count_)
    return;

  RendererMemoryMetrics memory_metrics;
  if (!GetRendererMemoryMetrics(&memory_metrics))
    return;
  RecordMemoryUsageAfterBackgroundedMB(
      "Memory.Experimental.Renderer.PartitionAlloc.AfterBackgrounded", suffix,
      memory_metrics.partition_alloc_kb / 1024);
  RecordMemoryUsageAfterBackgroundedMB(
      "Memory.Experimental.Renderer.BlinkGC.AfterBackgrounded", suffix,
      memory_metrics.blink_gc_kb / 1024);
  RecordMemoryUsageAfterBackgroundedMB(
      "Memory.Experimental.Renderer.Malloc.AfterBackgrounded", suffix,
      memory_metrics.malloc_mb);
  RecordMemoryUsageAfterBackgroundedMB(
      "Memory.Experimental.Renderer.Discardable.AfterBackgrounded", suffix,
      memory_metrics.discardable_kb / 1024);
  RecordMemoryUsageAfterBackgroundedMB(
      "Memory.Experimental.Renderer.V8MainThreaIsolate.AfterBackgrounded",
      suffix, memory_metrics.v8_main_thread_isolate_mb);
  RecordMemoryUsageAfterBackgroundedMB(
      "Memory.Experimental.Renderer.TotalAllocated.AfterBackgrounded", suffix,
      memory_metrics.total_allocated_mb);
}

#define GET_MEMORY_GROWTH(current, previous, allocator) \
  (current.allocator > previous.allocator               \
       ? current.allocator - previous.allocator         \
       : 0)

static void RecordBackgroundedRenderPurgeMemoryGrowthKB(const char* basename,
                                                        const char* suffix,
                                                        int memory_usage) {
  std::string histogram_name = base::StringPrintf("%s.%s", basename, suffix);
  base::UmaHistogramMemoryKB(histogram_name, memory_usage);
}

void RenderThreadImpl::OnRecordMetricsForBackgroundedRendererPurgeTimerExpired(
    const char* suffix,
    int foregrounded_count_when_purged) {
  // If this renderer is resumed, we should not update UMA.
  if (!RendererIsHidden())
    return;
  if (foregrounded_count_when_purged != process_foregrounded_count_)
    return;

  RendererMemoryMetrics memory_metrics;
  if (!GetRendererMemoryMetrics(&memory_metrics))
    return;

  RecordBackgroundedRenderPurgeMemoryGrowthKB(
      "PurgeAndSuspend.Experimental.MemoryGrowth.PartitionAllocKB", suffix,
      GET_MEMORY_GROWTH(memory_metrics, purge_and_suspend_memory_metrics_,
                        partition_alloc_kb));
  RecordBackgroundedRenderPurgeMemoryGrowthKB(
      "PurgeAndSuspend.Experimental.MemoryGrowth.BlinkGCKB", suffix,
      GET_MEMORY_GROWTH(memory_metrics, purge_and_suspend_memory_metrics_,
                        blink_gc_kb));
  RecordBackgroundedRenderPurgeMemoryGrowthKB(
      "PurgeAndSuspend.Experimental.MemoryGrowth.MallocKB", suffix,
      GET_MEMORY_GROWTH(memory_metrics, purge_and_suspend_memory_metrics_,
                        malloc_mb) *
          1024);
  RecordBackgroundedRenderPurgeMemoryGrowthKB(
      "PurgeAndSuspend.Experimental.MemoryGrowth.DiscardableKB", suffix,
      GET_MEMORY_GROWTH(memory_metrics, purge_and_suspend_memory_metrics_,
                        discardable_kb));
  RecordBackgroundedRenderPurgeMemoryGrowthKB(
      "PurgeAndSuspend.Experimental.MemoryGrowth.V8MainThreadIsolateKB", suffix,
      GET_MEMORY_GROWTH(memory_metrics, purge_and_suspend_memory_metrics_,
                        v8_main_thread_isolate_mb) *
          1024);
  RecordBackgroundedRenderPurgeMemoryGrowthKB(
      "PurgeAndSuspend.Experimental.MemoryGrowth.TotalAllocatedKB", suffix,
      GET_MEMORY_GROWTH(memory_metrics, purge_and_suspend_memory_metrics_,
                        total_allocated_mb) *
          1024);
}

void RenderThreadImpl::RecordMetricsForBackgroundedRendererPurge() {
  needs_to_record_first_active_paint_ = true;

  RendererMemoryMetrics memory_metrics;
  if (!GetRendererMemoryMetrics(&memory_metrics))
    return;

  purge_and_suspend_memory_metrics_ = memory_metrics;
  GetWebMainThreadScheduler()->DefaultTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &RenderThreadImpl::
              OnRecordMetricsForBackgroundedRendererPurgeTimerExpired,
          base::Unretained(this), "30min", process_foregrounded_count_),
      base::TimeDelta::FromMinutes(30));
  GetWebMainThreadScheduler()->DefaultTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &RenderThreadImpl::
              OnRecordMetricsForBackgroundedRendererPurgeTimerExpired,
          base::Unretained(this), "60min", process_foregrounded_count_),
      base::TimeDelta::FromMinutes(60));
  GetWebMainThreadScheduler()->DefaultTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &RenderThreadImpl::
              OnRecordMetricsForBackgroundedRendererPurgeTimerExpired,
          base::Unretained(this), "90min", process_foregrounded_count_),
      base::TimeDelta::FromMinutes(90));
}

void RenderThreadImpl::CompositingModeFallbackToSoftware() {
  gpu_->LoseChannel();
  is_gpu_compositing_disabled_ = true;
}

scoped_refptr<gpu::GpuChannelHost> RenderThreadImpl::EstablishGpuChannelSync() {
  TRACE_EVENT0("gpu", "RenderThreadImpl::EstablishGpuChannelSync");

  scoped_refptr<gpu::GpuChannelHost> gpu_channel =
      gpu_->EstablishGpuChannelSync();
  if (gpu_channel)
    GetContentClient()->SetGpuInfo(gpu_channel->gpu_info());
  return gpu_channel;
}

void RenderThreadImpl::RequestNewLayerTreeFrameSink(
    RenderWidget* render_widget,
    const GURL& url,
    LayerTreeFrameSinkCallback callback,
    const char* client_name) {
  const bool for_web_tests = blink::WebTestMode();
  // Misconfigured bots (eg. crbug.com/780757) could run web tests on a
  // machine where gpu compositing doesn't work. Don't crash in that case.
  if (for_web_tests && is_gpu_compositing_disabled_) {
    LOG(FATAL) << "Web tests require gpu compositing, but it is disabled.";
    return;
  }

  // TODO(jonross): Have this generated by the LayerTreeFrameSink itself, which
  // would then handle binding.
  mojo::PendingRemote<cc::mojom::RenderFrameMetadataObserver>
      render_frame_metadata_observer_remote;
  mojo::PendingRemote<cc::mojom::RenderFrameMetadataObserverClient>
      render_frame_metadata_client_remote;
  mojo::PendingReceiver<cc::mojom::RenderFrameMetadataObserverClient>
      render_frame_metadata_observer_client_receiver =
          render_frame_metadata_client_remote.InitWithNewPipeAndPassReceiver();
  auto render_frame_metadata_observer =
      std::make_unique<RenderFrameMetadataObserverImpl>(
          render_frame_metadata_observer_remote
              .InitWithNewPipeAndPassReceiver(),
          std::move(render_frame_metadata_client_remote));

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  cc::mojo_embedder::AsyncLayerTreeFrameSink::InitParams params;
  params.compositor_task_runner = compositor_task_runner_;
  if (for_web_tests && !compositor_task_runner_) {
    // The frame sink provider expects a compositor task runner, but we might
    // not have that if we're running web tests in single threaded mode.
    // Set it to be our thread's task runner instead.
    params.compositor_task_runner = main_thread_compositor_task_runner_;
  }

  // The renderer runs animations and layout for animate_only BeginFrames.
  params.wants_animate_only_begin_frames = true;

  // In disable frame rate limit mode, also let the renderer tick as fast as it
  // can. The top level begin frame source will also be running as a back to
  // back begin frame source, but using a synthetic begin frame source here
  // reduces latency when in this mode (at least for frames starting--it
  // potentially increases it for input on the other hand.)
  if (command_line.HasSwitch(switches::kDisableFrameRateLimit))
    params.synthetic_begin_frame_source = CreateSyntheticBeginFrameSource();

  params.client_name = client_name;

  mojo::PendingReceiver<viz::mojom::CompositorFrameSink>
      compositor_frame_sink_receiver = params.pipes.compositor_frame_sink_remote
                                           .InitWithNewPipeAndPassReceiver();
  mojo::PendingRemote<viz::mojom::CompositorFrameSinkClient>
      compositor_frame_sink_client;
  params.pipes.client_receiver =
      compositor_frame_sink_client.InitWithNewPipeAndPassReceiver();

  if (is_gpu_compositing_disabled_) {
    DCHECK(!for_web_tests);
    frame_sink_provider_->CreateForWidget(
        render_widget->routing_id(), std::move(compositor_frame_sink_receiver),
        std::move(compositor_frame_sink_client));
    frame_sink_provider_->RegisterRenderFrameMetadataObserver(
        render_widget->routing_id(),
        std::move(render_frame_metadata_observer_client_receiver),
        std::move(render_frame_metadata_observer_remote));
    std::move(callback).Run(
        std::make_unique<cc::mojo_embedder::AsyncLayerTreeFrameSink>(
            nullptr, nullptr, &params),
        std::move(render_frame_metadata_observer));
    return;
  }

  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host =
      EstablishGpuChannelSync();
  if (!gpu_channel_host) {
    // Wait and try again. We may hear that the compositing mode has switched
    // to software in the meantime.
    std::move(callback).Run(nullptr, nullptr);
    return;
  }

  scoped_refptr<viz::RasterContextProvider> worker_context_provider =
      SharedCompositorWorkerContextProvider(/*try_gpu_rasterization=*/true);
  if (!worker_context_provider) {
    // Cause the compositor to wait and try again.
    std::move(callback).Run(nullptr, nullptr);
    return;
  }

  // The renderer compositor context doesn't do a lot of stuff, so we don't
  // expect it to need a lot of space for commands or transfer. Raster and
  // uploads happen on the worker context instead.
  gpu::SharedMemoryLimits limits = gpu::SharedMemoryLimits::ForMailboxContext();

  // This is for an offscreen context for the compositor. So the default
  // framebuffer doesn't need alpha, depth, stencil, antialiasing.
  gpu::ContextCreationAttribs attributes;
  attributes.alpha_size = -1;
  attributes.depth_size = 0;
  attributes.stencil_size = 0;
  attributes.samples = 0;
  attributes.sample_buffers = 0;
  attributes.bind_generates_resource = false;
  attributes.lose_context_when_out_of_memory = true;
  attributes.enable_gles2_interface = true;
  attributes.enable_raster_interface = false;
  attributes.enable_oop_rasterization = false;

  constexpr bool automatic_flushes = false;
  constexpr bool support_locking = false;
  constexpr bool support_grcontext = true;

  scoped_refptr<viz::ContextProviderCommandBuffer> context_provider(
      new viz::ContextProviderCommandBuffer(
          gpu_channel_host, GetGpuMemoryBufferManager(), kGpuStreamIdDefault,
          kGpuStreamPriorityDefault, gpu::kNullSurfaceHandle, url,
          automatic_flushes, support_locking, support_grcontext, limits,
          attributes,
          viz::command_buffer_metrics::ContextType::RENDER_COMPOSITOR));

#if defined(OS_ANDROID)
  if (GetContentClient()->UsingSynchronousCompositing()) {
    // TODO(ericrk): Collapse with non-webview registration below.
    if (features::IsUsingVizFrameSubmissionForWebView()) {
      frame_sink_provider_->CreateForWidget(
          render_widget->routing_id(),
          std::move(compositor_frame_sink_receiver),
          std::move(compositor_frame_sink_client));
    }
    frame_sink_provider_->RegisterRenderFrameMetadataObserver(
        render_widget->routing_id(),
        std::move(render_frame_metadata_observer_client_receiver),
        std::move(render_frame_metadata_observer_remote));

    std::move(callback).Run(
        std::make_unique<SynchronousLayerTreeFrameSinkImpl>(
            std::move(context_provider), std::move(worker_context_provider),
            compositor_task_runner_, GetGpuMemoryBufferManager(),
            sync_message_filter(), g_next_layer_tree_frame_sink_id++,
            std::move(params.synthetic_begin_frame_source),
            render_widget->GetWebWidget()->GetSynchronousCompositorRegistry(),
            std::move(params.pipes.compositor_frame_sink_remote),
            std::move(params.pipes.client_receiver)),
        std::move(render_frame_metadata_observer));
    return;
  }
#endif
  frame_sink_provider_->CreateForWidget(
      render_widget->routing_id(), std::move(compositor_frame_sink_receiver),
      std::move(compositor_frame_sink_client));
  frame_sink_provider_->RegisterRenderFrameMetadataObserver(
      render_widget->routing_id(),
      std::move(render_frame_metadata_observer_client_receiver),
      std::move(render_frame_metadata_observer_remote));
  params.gpu_memory_buffer_manager = GetGpuMemoryBufferManager();
  std::move(callback).Run(
      std::make_unique<cc::mojo_embedder::AsyncLayerTreeFrameSink>(
          std::move(context_provider), std::move(worker_context_provider),
          &params),
      std::move(render_frame_metadata_observer));
}

blink::AssociatedInterfaceRegistry*
RenderThreadImpl::GetAssociatedInterfaceRegistry() {
  return &associated_interfaces_;
}

mojom::RenderMessageFilter* RenderThreadImpl::render_message_filter() {
  if (!render_message_filter_)
    GetChannel()->GetRemoteAssociatedInterface(&render_message_filter_);
  return render_message_filter_.get();
}

gpu::GpuChannelHost* RenderThreadImpl::GetGpuChannel() {
  return gpu_->GetGpuChannel().get();
}

void RenderThreadImpl::CreateView(mojom::CreateViewParamsPtr params,
                                  PassKey<AgentSchedulingGroup>) {
  CompositorDependencies* compositor_deps = this;
  is_scroll_animator_enabled_ = params->web_preferences.enable_scroll_animator;
  // TODO(crbug.com/1111231): For as long as views are created via the
  // `Renderer` interface (as opposed to `AgentSchedulingGroup`), we will always
  // have *exactly one* `AgentSchedulingGroup` in the process.
  DCHECK_EQ(agent_scheduling_groups_.size(), 1ul);
  AgentSchedulingGroup& agent_scheduling_group =
      *agent_scheduling_groups_.begin()->get();

  RenderViewImpl::Create(agent_scheduling_group, compositor_deps,
                         std::move(params), RenderWidget::ShowCallback(),
                         GetWebMainThreadScheduler()->DefaultTaskRunner());
}

void RenderThreadImpl::DestroyView(int32_t view_id,
                                   PassKey<AgentSchedulingGroup>) {
  RenderViewImpl* view = RenderViewImpl::FromRoutingID(view_id);
  DCHECK(view);

  // This IPC can be called from re-entrant contexts. We can't destroy a
  // RenderViewImpl while references still exist on the stack, so we dispatch a
  // non-nestable task. This method is called exactly once by the browser
  // process, and is used to release ownership of the corresponding
  // RenderViewImpl instance. https://crbug.com/1000035.
  base::ThreadTaskRunnerHandle::Get()->PostNonNestableTask(
      FROM_HERE,
      base::BindOnce(&RenderViewImpl::Destroy, base::Unretained(view)));
}

void RenderThreadImpl::CreateFrame(mojom::CreateFrameParamsPtr params,
                                   PassKey<AgentSchedulingGroup>) {
  CompositorDependencies* compositor_deps = this;
  mojo::PendingRemote<service_manager::mojom::InterfaceProvider>
      interface_provider(
          std::move(params->interface_bundle->interface_provider));
  mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
      browser_interface_broker(
          std::move(params->interface_bundle->browser_interface_broker));
  // TODO(crbug.com/1111231): For as long as frames are created via the
  // `Renderer` interface (as opposed to `AgentSchedulingGroup`), we will always
  // have *exactly one* `AgentSchedulingGroup` in the process.
  DCHECK_EQ(agent_scheduling_groups_.size(), 1ul);
  AgentSchedulingGroup& agent_scheduling_group =
      *agent_scheduling_groups_.begin()->get();

  RenderFrameImpl::CreateFrame(
      agent_scheduling_group, params->routing_id, std::move(interface_provider),
      std::move(browser_interface_broker), params->previous_routing_id,
      params->opener_frame_token, params->parent_routing_id,
      params->previous_sibling_routing_id, params->frame_token,
      params->devtools_frame_token, params->replication_state, compositor_deps,
      std::move(params->widget_params),
      std::move(params->frame_owner_properties),
      params->has_committed_real_load);
}

void RenderThreadImpl::CreateAgentSchedulingGroup(
    mojo::PendingRemote<mojom::AgentSchedulingGroupHost>
        agent_scheduling_group_host,
    mojo::PendingReceiver<mojom::AgentSchedulingGroup> agent_scheduling_group) {
  agent_scheduling_groups_.emplace(std::make_unique<AgentSchedulingGroup>(
      *this, std::move(agent_scheduling_group_host),
      std::move(agent_scheduling_group)));
}

void RenderThreadImpl::CreateAssociatedAgentSchedulingGroup(
    mojo::PendingAssociatedRemote<mojom::AgentSchedulingGroupHost>
        agent_scheduling_group_host,
    mojo::PendingAssociatedReceiver<mojom::AgentSchedulingGroup>
        agent_scheduling_group) {
  agent_scheduling_groups_.emplace(std::make_unique<AgentSchedulingGroup>(
      *this, std::move(agent_scheduling_group_host),
      std::move(agent_scheduling_group)));
}

void RenderThreadImpl::CreateFrameProxy(
    int32_t routing_id,
    int32_t render_view_routing_id,
    const base::Optional<base::UnguessableToken>& opener_frame_token,
    int32_t parent_routing_id,
    const FrameReplicationState& replicated_state,
    const base::UnguessableToken& frame_token,
    const base::UnguessableToken& devtools_frame_token,
    PassKey<AgentSchedulingGroup>) {
  // TODO(crbug.com/1111231): For as long as frame proxies are created via the
  // `Renderer` interface (as opposed to `AgentSchedulingGroup`), we will always
  // have *exactly one* `AgentSchedulingGroup` in the process.
  DCHECK_EQ(agent_scheduling_groups_.size(), 1ul);
  AgentSchedulingGroup& agent_scheduling_group =
      *agent_scheduling_groups_.begin()->get();

  RenderFrameProxy::CreateFrameProxy(agent_scheduling_group, routing_id,
                                     render_view_routing_id, opener_frame_token,
                                     parent_routing_id, replicated_state,
                                     frame_token, devtools_frame_token);
}

void RenderThreadImpl::OnNetworkConnectionChanged(
    net::NetworkChangeNotifier::ConnectionType type,
    double max_bandwidth_mbps) {
  online_status_ = type != net::NetworkChangeNotifier::CONNECTION_NONE;
  WebNetworkStateNotifier::SetOnLine(online_status_);
  if (url_loader_throttle_provider_)
    url_loader_throttle_provider_->SetOnline(online_status_);
  for (auto& observer : observers_)
    observer.NetworkStateChanged(online_status_);
  WebNetworkStateNotifier::SetWebConnection(
      NetConnectionTypeToWebConnectionType(type), max_bandwidth_mbps);
}

void RenderThreadImpl::OnNetworkQualityChanged(
    net::EffectiveConnectionType type,
    base::TimeDelta http_rtt,
    base::TimeDelta transport_rtt,
    double downlink_throughput_kbps) {
  LOCAL_HISTOGRAM_BOOLEAN("NQE.RenderThreadNotified", true);
  WebNetworkStateNotifier::SetNetworkQuality(
      EffectiveConnectionTypeToWebEffectiveConnectionType(type), http_rtt,
      transport_rtt, downlink_throughput_kbps);
}

void RenderThreadImpl::SetWebKitSharedTimersSuspended(bool suspend) {
#if defined(OS_ANDROID)
  if (suspend) {
    main_thread_scheduler_->PauseTimersForAndroidWebView();
  } else {
    main_thread_scheduler_->ResumeTimersForAndroidWebView();
  }
#else
  NOTREACHED();
#endif
}

void RenderThreadImpl::SetUserAgent(const std::string& user_agent) {
  DCHECK(user_agent_.IsNull());
  user_agent_ = WebString::FromUTF8(user_agent);
  GetContentClient()->renderer()->DidSetUserAgent(user_agent);
}

void RenderThreadImpl::SetUserAgentMetadata(
    const blink::UserAgentMetadata& user_agent_metadata) {
  user_agent_metadata_ = user_agent_metadata;
}

void RenderThreadImpl::SetCorsExemptHeaderList(
    const std::vector<std::string>& list) {
  resource_dispatcher_->SetCorsExemptHeaderList(list);
}

void RenderThreadImpl::UpdateScrollbarTheme(
    mojom::UpdateScrollbarThemeParamsPtr params) {
#if defined(OS_MAC)
  blink::WebScrollbarTheme::UpdateScrollbarsWithNSDefaults(
      params->has_initial_button_delay
          ? base::make_optional(params->initial_button_delay)
          : base::nullopt,
      params->has_autoscroll_button_delay
          ? base::make_optional(params->autoscroll_button_delay)
          : base::nullopt,
      params->preferred_scroller_style, params->redraw,
      params->jump_on_track_click);

  is_elastic_overscroll_enabled_ = params->scroll_view_rubber_banding;
#else
  NOTREACHED();
#endif
}

void RenderThreadImpl::OnSystemColorsChanged(
    int32_t aqua_color_variant,
    const std::string& highlight_text_color,
    const std::string& highlight_color) {
#if defined(OS_MAC)
  SystemColorsDidChange(aqua_color_variant, highlight_text_color,
                        highlight_color);
#else
  NOTREACHED();
#endif
}

void RenderThreadImpl::UpdateSystemColorInfo(
    mojom::UpdateSystemColorInfoParamsPtr params) {
  bool did_system_color_info_change =
      ui::NativeTheme::GetInstanceForWeb()->UpdateSystemColorInfo(
          params->is_dark_mode, params->is_high_contrast, params->colors);
  if (did_system_color_info_change) {
    blink::SystemColorsChanged();
    blink::ColorSchemeChanged();
  }
}

void RenderThreadImpl::PurgePluginListCache(bool reload_pages) {
#if BUILDFLAG(ENABLE_PLUGINS)
  blink::ResetPluginCache(reload_pages);

  for (auto& observer : observers_)
    observer.PluginListChanged();
#else
  NOTREACHED();
#endif
}

void RenderThreadImpl::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  TRACE_EVENT1("memory", "RenderThreadImpl::OnMemoryPressure", "level",
               memory_pressure_level);
  if (blink_platform_impl_) {
    blink::WebMemoryPressureListener::OnMemoryPressure(
        static_cast<blink::WebMemoryPressureLevel>(memory_pressure_level));
  }
  if (memory_pressure_level ==
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    ReleaseFreeMemory();
  }
}

void RenderThreadImpl::GetRoute(
    int32_t routing_id,
    mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterfaceProvider>
        receiver) {
  associated_interface_provider_receivers_.Add(this, std::move(receiver),
                                               routing_id);
}

void RenderThreadImpl::GetAssociatedInterface(
    const std::string& name,
    mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterface>
        receiver) {
  int32_t routing_id =
      associated_interface_provider_receivers_.current_context();
  // We delegate to ChildThreadImpl when we actually need to communicate with
  // IPC::Listeners, since it owns the router.
  ChildThreadImpl::GetAssociatedInterface(routing_id, name,
                                          std::move(receiver));
}

scoped_refptr<base::SingleThreadTaskRunner>
RenderThreadImpl::GetMediaThreadTaskRunner() {
  DCHECK(main_thread_runner()->BelongsToCurrentThread());
  if (!media_thread_) {
    media_thread_.reset(new base::Thread("Media"));
#if defined(OS_FUCHSIA)
    // Start IO thread on Fuchsia to make that thread usable for FIDL.
    base::Thread::Options options(base::MessagePumpType::IO, 0);
#else
    base::Thread::Options options;
#endif
    media_thread_->StartWithOptions(options);
  }
  return media_thread_->task_runner();
}

base::TaskRunner* RenderThreadImpl::GetWorkerTaskRunner() {
  return categorized_worker_pool_.get();
}

scoped_refptr<viz::RasterContextProvider>
RenderThreadImpl::SharedCompositorWorkerContextProvider(
    bool try_gpu_rasterization) {
  DCHECK(IsMainThread());
  // Try to reuse existing shared worker context provider.
  if (shared_worker_context_provider_) {
    // Note: If context is lost, delete reference after releasing the lock.
    viz::RasterContextProvider::ScopedRasterContextLock lock(
        shared_worker_context_provider_.get());
    if (lock.RasterInterface()->GetGraphicsResetStatusKHR() == GL_NO_ERROR)
      return shared_worker_context_provider_;
  }

  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host(
      EstablishGpuChannelSync());
  if (!gpu_channel_host) {
    shared_worker_context_provider_ = nullptr;
    return shared_worker_context_provider_;
  }

  bool support_locking = true;
  bool support_oop_rasterization =
      gpu_channel_host->gpu_feature_info()
          .status_values[gpu::GPU_FEATURE_TYPE_OOP_RASTERIZATION] ==
      gpu::kGpuFeatureStatusEnabled;
  bool support_gpu_rasterization =
      try_gpu_rasterization && !support_oop_rasterization &&
      gpu_channel_host->gpu_feature_info()
              .status_values[gpu::GPU_FEATURE_TYPE_GPU_RASTERIZATION] ==
          gpu::kGpuFeatureStatusEnabled;
  bool support_gles2_interface = support_gpu_rasterization;
  bool support_raster_interface = true;
  bool support_grcontext = support_gpu_rasterization;
  bool automatic_flushes = false;
  auto shared_memory_limits =
      support_oop_rasterization ? gpu::SharedMemoryLimits::ForOOPRasterContext()
                                : gpu::SharedMemoryLimits();
  shared_worker_context_provider_ = CreateOffscreenContext(
      std::move(gpu_channel_host), GetGpuMemoryBufferManager(),
      shared_memory_limits, support_locking, support_gles2_interface,
      support_raster_interface, support_oop_rasterization, support_grcontext,
      automatic_flushes,
      viz::command_buffer_metrics::ContextType::RENDER_WORKER,
      kGpuStreamIdWorker, kGpuStreamPriorityWorker);
  auto result = shared_worker_context_provider_->BindToCurrentThread();
  if (result != gpu::ContextResult::kSuccess) {
    shared_worker_context_provider_ = nullptr;
    return nullptr;
  }

  // Check if we really have support for GPU rasterization.
  if (support_gpu_rasterization) {
    bool really_support_gpu_rasterization = false;
    {
      viz::RasterContextProvider::ScopedRasterContextLock scoped_context(
          shared_worker_context_provider_.get());
      if (shared_worker_context_provider_->ContextCapabilities()
              .gpu_rasterization &&
          shared_worker_context_provider_->ContextSupport()
              ->HasGrContextSupport()) {
        // Do not check GrContext above. It is lazy-created, and we only want to
        // create it if it might be used.
        GrDirectContext* gr_context =
            shared_worker_context_provider_->GrContext();
        really_support_gpu_rasterization = !!gr_context;
      }
    }

    // If not really supported, recreate context with different attributes.
    if (!really_support_gpu_rasterization) {
      shared_worker_context_provider_ = nullptr;
      return SharedCompositorWorkerContextProvider(
          /*try_gpu_rasterization=*/false);
    }
  }
  return shared_worker_context_provider_;
}

bool RenderThreadImpl::RendererIsHidden() const {
  return visible_state_ == mojom::RenderProcessVisibleState::kHidden;
}

void RenderThreadImpl::OnRendererHidden() {
  blink::MainThreadIsolate()->IsolateInBackgroundNotification();
  // TODO(rmcilroy): Remove IdleHandler and replace it with an IdleTask
  // scheduled by the RendererScheduler - http://crbug.com/469210.
  if (!GetContentClient()->renderer()->RunIdleHandlerWhenWidgetsHidden())
    return;
  main_thread_scheduler_->SetRendererHidden(true);
}

void RenderThreadImpl::OnRendererVisible() {
  blink::MainThreadIsolate()->IsolateInForegroundNotification();
  if (!GetContentClient()->renderer()->RunIdleHandlerWhenWidgetsHidden())
    return;
  main_thread_scheduler_->SetRendererHidden(false);
}

bool RenderThreadImpl::RendererIsBackgrounded() const {
  return background_state_ ==
         mojom::RenderProcessBackgroundState::kBackgrounded;
}

void RenderThreadImpl::OnRendererBackgrounded() {
  main_thread_scheduler_->SetRendererBackgrounded(true);
  needs_to_record_first_active_paint_ = false;
  discardable_memory_allocator_->OnBackgrounded();
  GetWebMainThreadScheduler()->DefaultTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&RenderThreadImpl::RecordMemoryUsageAfterBackgrounded,
                     base::Unretained(this), "5min",
                     process_foregrounded_count_),
      base::TimeDelta::FromMinutes(5));
  GetWebMainThreadScheduler()->DefaultTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&RenderThreadImpl::RecordMemoryUsageAfterBackgrounded,
                     base::Unretained(this), "10min",
                     process_foregrounded_count_),
      base::TimeDelta::FromMinutes(10));
  GetWebMainThreadScheduler()->DefaultTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&RenderThreadImpl::RecordMemoryUsageAfterBackgrounded,
                     base::Unretained(this), "15min",
                     process_foregrounded_count_),
      base::TimeDelta::FromMinutes(15));
  was_backgrounded_time_ = base::TimeTicks::Now();
}

void RenderThreadImpl::OnRendererForegrounded() {
  main_thread_scheduler_->SetRendererBackgrounded(false);
  discardable_memory_allocator_->OnForegrounded();
  process_foregrounded_count_++;
}

void RenderThreadImpl::ReleaseFreeMemory() {
  TRACE_EVENT0("blink", "RenderThreadImpl::ReleaseFreeMemory()");
  base::allocator::ReleaseFreeMemory();
  discardable_memory_allocator_->ReleaseFreeMemory();

  // Do not call into blink if it is not initialized.
  if (blink_platform_impl_) {
    // Purge Skia font cache, resource cache, and image filter.
    SkGraphics::PurgeAllCaches();
    blink::WebMemoryPressureListener::OnPurgeMemory();
  }
}

void RenderThreadImpl::OnSyncMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  if (!blink::MainThreadIsolate())
    return;

  v8::MemoryPressureLevel v8_memory_pressure_level =
      static_cast<v8::MemoryPressureLevel>(memory_pressure_level);

#if !BUILDFLAG(ALLOW_CRITICAL_MEMORY_PRESSURE_HANDLING_IN_FOREGROUND)
  // In order to reduce performance impact, translate critical level to
  // moderate level for foreground renderer.
  if (!RendererIsHidden() &&
      v8_memory_pressure_level == v8::MemoryPressureLevel::kCritical)
    v8_memory_pressure_level = v8::MemoryPressureLevel::kModerate;
#endif  // !BUILDFLAG(ALLOW_CRITICAL_MEMORY_PRESSURE_HANDLING_IN_FOREGROUND)

  blink::MainThreadIsolate()->MemoryPressureNotification(
      v8_memory_pressure_level);
  blink::MemoryPressureNotificationToWorkerThreadIsolates(
      v8_memory_pressure_level);
}

void RenderThreadImpl::OnRouteProviderReceiver(
    mojo::PendingAssociatedReceiver<mojom::RouteProvider> receiver) {
  DCHECK(!route_provider_receiver_.is_bound());
  route_provider_receiver_.Bind(
      std::move(receiver),
      GetWebMainThreadScheduler()->DeprecatedDefaultTaskRunner());
}

void RenderThreadImpl::OnRendererInterfaceReceiver(
    mojo::PendingAssociatedReceiver<mojom::Renderer> receiver) {
  DCHECK(!renderer_receiver_.is_bound());
  renderer_receiver_.Bind(
      std::move(receiver),
      GetWebMainThreadScheduler()->DeprecatedDefaultTaskRunner());
}

bool RenderThreadImpl::NeedsToRecordFirstActivePaint(
    int ttfap_metric_type) const {
  if (ttfap_metric_type == RenderWidget::TTFAP_AFTER_PURGED)
    return needs_to_record_first_active_paint_;

  if (was_backgrounded_time_.is_min())
    return false;
  base::TimeDelta passed = base::TimeTicks::Now() - was_backgrounded_time_;
  return passed.InMinutes() >= 5;
}

void RenderThreadImpl::SetRenderingColorSpace(
    const gfx::ColorSpace& color_space) {
  DCHECK(IsMainThread());
  rendering_color_space_ = color_space;

  for (const auto& factories : gpu_factories_) {
    if (factories)
      factories->SetRenderingColorSpace(color_space);
  }
}

RenderThreadImpl::UnfreezableMessageFilter::UnfreezableMessageFilter(
    RenderThreadImpl* render_thread_impl)
    : render_thread_impl_(render_thread_impl) {}

// Called on the I/O thread.
bool RenderThreadImpl::UnfreezableMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  if ((IPC_MESSAGE_CLASS(message) == UnfreezableFrameMsgStart) ||
      (IPC_MESSAGE_CLASS(message) == PageMsgStart)) {
    auto task_runner = GetUnfreezableTaskRunner(message.routing_id());
    if (task_runner) {
      return task_runner->PostTask(
          FROM_HERE,
          base::BindOnce(
              base::IgnoreResult(&RenderThreadImpl::OnMessageReceived),
              base::Unretained(render_thread_impl_), message));
    }
  }
  // If unfreezable task runner is not found or the message class is not
  // UnfreezableFrameMsgStart, return false so that this filter is skipped and
  // other handlers can continue executing and handle this message.
  return false;
}

// Called on the listener thread.
void RenderThreadImpl::UnfreezableMessageFilter::
    AddListenerUnfreezableTaskRunner(
        int32_t routing_id,
        scoped_refptr<base::SingleThreadTaskRunner> unfreezable_task_runner) {
  DCHECK(unfreezable_task_runner);
  base::AutoLock lock(unfreezable_task_runners_lock_);
  DCHECK(!base::Contains(unfreezable_task_runners_, routing_id));
  unfreezable_task_runners_.emplace(routing_id,
                                    std::move(unfreezable_task_runner));
}

// Called on the listener thread.
void RenderThreadImpl::UnfreezableMessageFilter::
    RemoveListenerUnfreezableTaskRunner(int32_t routing_id) {
  base::AutoLock lock(unfreezable_task_runners_lock_);
  unfreezable_task_runners_.erase(routing_id);
}

// Called on the I/O thread.
scoped_refptr<base::SingleThreadTaskRunner>
RenderThreadImpl::UnfreezableMessageFilter::GetUnfreezableTaskRunner(
    int32_t routing_id) {
  base::AutoLock lock(unfreezable_task_runners_lock_);
  auto it = unfreezable_task_runners_.find(routing_id);
  if (it != unfreezable_task_runners_.end())
    return it->second;
  return nullptr;
}

RenderThreadImpl::UnfreezableMessageFilter::~UnfreezableMessageFilter() {}

}  // namespace content
