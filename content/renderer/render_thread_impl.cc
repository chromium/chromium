// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_thread_impl.h"

#include <limits>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/allocator/allocator_extension.h"
#include "base/allocator/partition_alloc_support.h"
#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/debug/crash_logging.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/discardable_memory_allocator.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/observer_list.h"
#include "base/path_service.h"
#include "base/process/process_metrics.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_pressure_level_proto.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/base/histograms.h"
#include "cc/base/switches.h"
#include "cc/mojo_embedder/async_layer_tree_frame_sink.h"
#include "cc/raster/task_graph_runner.h"
#include "cc/tiles/image_decode_cache_utils.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/raster_context_provider_wrapper.h"
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
#include "content/common/content_switches_internal.h"
#include "content/common/main_frame_counter.h"
#include "content/common/process_visibility_tracker.h"
#include "content/common/pseudonymization_salt.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/gpu_stream_constants.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread_observer.h"
#include "content/renderer/agent_scheduling_group.h"
#include "content/renderer/browser_exposed_renderer_interfaces.h"
#include "content/renderer/effective_connection_type_helper.h"
#include "content/renderer/media/codec_factory.h"
#include "content/renderer/media/gpu/gpu_video_accelerator_factories_impl.h"
#include "content/renderer/media/media_factory.h"
#include "content/renderer/media/render_media_client.h"
#include "content/renderer/net_info_helper.h"
#include "content/renderer/render_process_impl.h"
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
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_channel_mojo.h"
#include "ipc/ipc_platform_file.h"
#include "media/base/decoder_factory.h"
#include "media/base/media.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"
#include "media/renderers/default_decoder_factory.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "net/base/net_errors.h"
#include "net/base/port_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "services/viz/public/cpp/gpu/gpu.h"
#include "skia/ext/skia_memory_dump_provider.h"
#include "third_party/abseil-cpp/absl/base/attributes.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/origin_trials/origin_trials_settings_provider.h"
#include "third_party/blink/public/common/page/launching_process_state.h"
#include "third_party/blink/public/common/privacy_budget/active_sampling.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/mojom/origin_trials/origin_trials_settings.mojom.h"
#include "third_party/blink/public/platform/modules/video_capture/web_video_capture_impl_manager.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/platform/web_cache.h"
#include "third_party/blink/public/platform/web_image_generator.h"
#include "third_party/blink/public/platform/web_memory_pressure_listener.h"
#include "third_party/blink/public/platform/web_network_state_notifier.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/platform/web_scoped_page_pauser.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_theme_engine.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_render_theme.h"
#include "third_party/blink/public/web/web_security_policy.h"
#include "third_party/blink/public/web/web_user_level_memory_pressure_signal_generator.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/display/display_switches.h"
#include "v8/include/v8-extension.h"

#if BUILDFLAG(IS_ANDROID)
#include <cpu-features.h>
#include "content/renderer/media/android/stream_texture_factory.h"
#include "media/base/android/media_codec_util.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#include "content/renderer/theme_helper_mac.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <objbase.h>
#include <windows.h>
#include "content/renderer/media/win/dcomp_texture_factory.h"
#include "content/renderer/media/win/overlay_state_service_provider.h"
#include "media/base/win/mf_feature_checks.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "content/renderer/renderer_thread_type_handler.h"
#endif

#ifdef ENABLE_VTUNE_JIT_INTERFACE
#include "v8/src/third_party/vtune/v8-vtune.h"
#endif

#if defined(ENABLE_IPC_FUZZER)
#include "content/common/external_ipc_dumper.h"
#include "mojo/public/cpp/bindings/message_dumper.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif

#if BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)
#include "content/renderer/media/codec_factory_mojo.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#endif

#if BUILDFLAG(IS_FUCHSIA)
#include "content/renderer/media/codec_factory_fuchsia.h"
#include "media/mojo/mojom/fuchsia_media.mojom.h"
#endif

#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
#include "base/test/clang_profiling.h"
#endif

namespace content {

namespace {

using ::base::PassKey;
using ::blink::WebDocument;
using ::blink::WebFrame;
using ::blink::WebNetworkStateNotifier;
using ::blink::WebRuntimeFeatures;
using ::blink::WebSecurityPolicy;
using ::blink::WebString;
using ::blink::WebView;

// An implementation of mojom::RenderMessageFilter which can be mocked out
// for tests which may indirectly send messages over this interface.
mojom::RenderMessageFilter* g_render_message_filter_for_testing;

// An implementation of RendererBlinkPlatformImpl which can be mocked out
// for tests.
RendererBlinkPlatformImpl* g_current_blink_platform_impl_for_testing;

// Keep the global RenderThreadImpl in a TLS slot so it is impossible to access
// incorrectly from the wrong thread.
ABSL_CONST_INIT thread_local RenderThreadImpl* render_thread = nullptr;

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

// Feature to migrate the Media thread to a SequencedTaskRunner backed from
// the base::ThreadPool. Does not currently work on Fuchsia due to FIDL
// requiring thread affinity.
BASE_DECLARE_FEATURE(kUseThreadPoolForMediaTaskRunner){
    "UseThreadPoolForMediaTaskRunner", base::FEATURE_DISABLED_BY_DEFAULT};

void AddCrashKey(v8::CrashKeyId id, const std::string& value) {
  using base::debug::AllocateCrashKeyString;
  using base::debug::CrashKeySize;
  using base::debug::SetCrashKeyString;

  switch (id) {
    case v8::CrashKeyId::kIsolateAddress:
      static auto* const isolate_address =
          AllocateCrashKeyString("v8_isolate_address", CrashKeySize::Size32);
      SetCrashKeyString(isolate_address, value);
      break;
    case v8::CrashKeyId::kReadonlySpaceFirstPageAddress:
      static auto* const ro_space_firstpage_address = AllocateCrashKeyString(
          "v8_ro_space_firstpage_address", CrashKeySize::Size32);
      SetCrashKeyString(ro_space_firstpage_address, value);
      break;
    case v8::CrashKeyId::kMapSpaceFirstPageAddress:
      static auto* const map_space_firstpage_address = AllocateCrashKeyString(
          "v8_map_space_firstpage_address", CrashKeySize::Size32);
      SetCrashKeyString(map_space_firstpage_address, value);
      break;
    case v8::CrashKeyId::kCodeSpaceFirstPageAddress:
      static auto* const code_space_firstpage_address = AllocateCrashKeyString(
          "v8_code_space_firstpage_address", CrashKeySize::Size32);
      SetCrashKeyString(code_space_firstpage_address, value);
      break;
    case v8::CrashKeyId::kDumpType:
      static auto* const dump_type =
          AllocateCrashKeyString("dump-type", CrashKeySize::Size32);
      SetCrashKeyString(dump_type, value);
      break;
    default:
      // Doing nothing for new keys is a valid option. Having this case allows
      // to introduce new CrashKeyId's without triggering a build break.
      break;
  }
}

// Updates the crash key for whether this renderer is foregrounded.
void UpdateForegroundCrashKey(bool foreground) {
  static auto* const crash_key = base::debug::AllocateCrashKeyString(
      "renderer_foreground", base::debug::CrashKeySize::Size32);
  base::debug::SetCrashKeyString(crash_key, foreground ? "true" : "false");
}

scoped_refptr<viz::ContextProviderCommandBuffer> CreateOffscreenContext(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
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
      std::move(gpu_channel_host), stream_id, stream_priority,
      gpu::kNullSurfaceHandle,
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

static bool IsSingleProcess() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSingleProcess);
}

}  // namespace

RenderThreadImpl::HistogramCustomizer::HistogramCustomizer() {
  custom_histograms_.insert("V8.MemoryHeapSampleTotalCommitted");
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
      base::Contains(custom_histograms_, name)) {
    name += common_host_histogram_suffix_;
  }
  return name;
}

void RenderThreadImpl::HistogramCustomizer::SetCommonHost(
    const std::string& host) {
  if (host != common_host_) {
    common_host_ = host;
    common_host_histogram_suffix_ = HostToCustomHistogramSuffix(host);
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
  return render_thread;
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
      client_id_(GetClientIdFromCommandLine()) {
  TRACE_EVENT0("startup", "RenderThreadImpl::Create");
  Init();
}

void RenderThreadImpl::Init() {
  TRACE_EVENT0("startup", "RenderThreadImpl::Init");

  SCOPED_UMA_HISTOGRAM_TIMER("Renderer.RenderThreadImpl.Init");

  GetContentClient()->renderer()->PostIOThreadCreated(GetIOTaskRunner().get());

  base::trace_event::TraceLog::GetInstance()->SetThreadSortIndex(
      base::PlatformThread::CurrentId(),
      kTraceEventRendererMainThreadSortIndex);

#if BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
  // On Mac and Android Java UI, the select popups are rendered by the browser.
#if BUILDFLAG(IS_MAC)
  // When UseCommonSelectPopup is enabled, the internal popup menu should be
  // used.
  if (!features::IsUseCommonSelectPopupEnabled())
#endif
    blink::WebView::SetUseExternalPopupMenus(true);
#endif

  render_thread = this;
  g_main_task_runner.Get() = base::SingleThreadTaskRunner::GetCurrentDefault();

  // Register this object as the main thread.
  ChildProcess::current()->set_main_thread(this);

  metrics::InitializeSingleSampleMetricsFactory(base::BindRepeating(
      &CreateSingleSampleMetricsProvider, child_process_host()));

  mojo::PendingRemote<viz::mojom::Gpu> remote_gpu;
  BindHostReceiver(remote_gpu.InitWithNewPipeAndPassReceiver());
  gpu_ = viz::Gpu::Create(std::move(remote_gpu), GetIOTaskRunner());

  // Establish the GPU channel now, so its ready when needed and we don't have
  // to wait on a sync call.
  if (base::FeatureList::IsEnabled(features::kEarlyEstablishGpuChannel)) {
    gpu_->EstablishGpuChannel(
        base::BindOnce([](scoped_refptr<gpu::GpuChannelHost> host) {
          if (host)
            GetContentClient()->SetGpuInfo(host->gpu_info());
        }));
  }

  // NOTE: Do not add interfaces to |binders| within this method. Instead,
  // modify the definition of |ExposeRendererInterfacesToBrowser()| to ensure
  // security review coverage.
  mojo::BinderMap binders;
  InitializeWebKit(&binders);

  vc_manager_ = std::make_unique<blink::WebVideoCaptureImplManager>();

  GetContentClient()->renderer()->RenderThreadStarted();
  ExposeRendererInterfacesToBrowser(weak_factory_.GetWeakPtr(), &binders);
  ExposeInterfacesToBrowser(std::move(binders));

  url_loader_throttle_provider_ =
      GetContentClient()->renderer()->CreateURLLoaderThrottleProvider(
          blink::URLLoaderThrottleProviderType::kFrame);

  GetAssociatedInterfaceRegistry()->AddInterface<mojom::Renderer>(
      base::BindRepeating(&RenderThreadImpl::OnRendererInterfaceReceiver,
                          base::Unretained(this)));

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

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  RendererThreadTypeHandler::NotifyRenderThreadCreated();
#endif

  cc::SetClientNameForMetrics("Renderer");

  is_threaded_animation_enabled_ =
      !command_line.HasSwitch(cc::switches::kDisableThreadedAnimation);

  is_elastic_overscroll_enabled_ = switches::IsElasticOverscrollEnabled();

  if (command_line.HasSwitch(switches::kDisableLCDText)) {
    is_lcd_text_enabled_ = false;
  } else if (command_line.HasSwitch(switches::kEnableLCDText)) {
    is_lcd_text_enabled_ = true;
  } else {
#if BUILDFLAG(IS_ANDROID)
    is_lcd_text_enabled_ = false;
#elif BUILDFLAG(IS_MAC)
    is_lcd_text_enabled_ = IsSubpixelAntialiasingAvailable();
#else
    is_lcd_text_enabled_ = true;
#endif
  }

  if (command_line.HasSwitch(switches::kDisableGpuCompositing))
    is_gpu_compositing_disabled_ = true;

  // Note that under Linux, the media library will normally already have
  // been initialized by the Zygote before this instance became a Renderer.
  media::InitializeMediaLibrary();

  memory_pressure_listener_ = std::make_unique<base::MemoryPressureListener>(
      FROM_HERE,
      base::BindRepeating(&RenderThreadImpl::OnMemoryPressure,
                          base::Unretained(this)),
      base::BindRepeating(&RenderThreadImpl::OnSyncMemoryPressure,
                          base::Unretained(this)));

  discardable_memory_allocator_ = CreateDiscardableMemoryAllocator();

  // TODO(boliu): In single process, browser main loop should set up the
  // discardable memory manager, and should skip this if kSingleProcess.
  // See crbug.com/503724.
  base::DiscardableMemoryAllocator::SetInstance(
      discardable_memory_allocator_.get());

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  render_message_filter()->SetThreadType(
      ChildProcess::current()->io_thread_id(), base::ThreadType::kCompositing);
#endif

  process_foregrounded_count_ = 0;

  if (!is_gpu_compositing_disabled_) {
    BindHostReceiver(compositing_mode_reporter_.BindNewPipeAndPassReceiver());

    compositing_mode_reporter_->AddCompositingModeWatcher(
        compositing_mode_watcher_receiver_.BindNewPipeAndPassRemote(
            main_thread_scheduler_->CompositorTaskRunner()));
  }

  variations_observer_ = std::make_unique<VariationsRenderThreadObserver>();
  AddObserver(variations_observer_.get());

  base::ThreadPool::PostTask(FROM_HERE,
                             base::BindOnce([] { SkFontMgr::RefDefault(); }));

  bool should_actively_sample_fonts =
      command_line.HasSwitch(kFirstRendererProcess) &&
      blink::IdentifiabilityStudySettings::Get()->ShouldActivelySample() &&
      !blink::IdentifiabilityStudySettings::Get()
           ->FontFamiliesToActivelySample()
           .empty();
  if (should_actively_sample_fonts) {
    mojo::PendingRemote<ukm::mojom::UkmRecorderFactory> pending_factory;
    RenderThread::Get()->BindHostReceiver(
        pending_factory.InitWithNewPipeAndPassReceiver());
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner =
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
             base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
    sequenced_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](mojo::PendingRemote<ukm::mojom::UkmRecorderFactory>
                   pending_factory) {
              mojo::Remote<ukm::mojom::UkmRecorderFactory> factory(
                  std::move(pending_factory));
              auto ukm_recorder = ukm::MojoUkmRecorder::Create(*factory);
              blink::IdentifiabilityActiveSampler::ActivelySampleAvailableFonts(
                  ukm_recorder.get());
            },
            std::move(pending_factory)));
  }
  UpdateForegroundCrashKey(
      /*foreground=*/!blink::kLaunchingProcessIsBackgrounded);
}

RenderThreadImpl::~RenderThreadImpl() {
  g_main_task_runner.Get() = nullptr;

  // Need to make sure this reference is removed on the correct task runner;
  if (video_frame_compositor_thread_ &&
      video_frame_compositor_context_provider_) {
    video_frame_compositor_thread_->task_runner()->ReleaseSoon(
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

  blink::LogStatsDuringShutdown();

  // In a single-process mode, we cannot call _exit(0) in Shutdown() because
  // it will exit the process before the browser side is ready to exit.
  if (!IsSingleProcess())
    base::Process::TerminateCurrentProcessImmediately(0);
}

bool RenderThreadImpl::ShouldBeDestroyed() {
  DCHECK(IsSingleProcess());
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
}

void RenderThreadImpl::AttachTaskRunnerToRoute(
    int32_t routing_id,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  GetChannel()->AddListenerTaskRunner(routing_id, std::move(task_runner));
}

void RenderThreadImpl::RemoveRoute(int32_t routing_id) {
  ChildThreadImpl::GetRouter()->RemoveRoute(routing_id);
  GetChannel()->RemoveListenerTaskRunner(routing_id);
  pending_frames_.erase(routing_id);
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

bool RenderThreadImpl::GenerateFrameRoutingID(
    int32_t& routing_id,
    blink::LocalFrameToken& frame_token,
    base::UnguessableToken& devtools_frame_token,
    blink::DocumentToken& document_token) {
  return render_message_filter()->GenerateFrameRoutingID(
      &routing_id, &frame_token, &devtools_frame_token, &document_token);
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

void RenderThreadImpl::InitializeCompositorThread() {
  blink_platform_impl_->CreateAndSetCompositorThread();
  compositor_task_runner_ = blink_platform_impl_->CompositorThreadTaskRunner();

  compositor_task_runner_->PostTask(FROM_HERE,
                                    base::BindOnce(&base::DisallowBlocking));
  GetContentClient()->renderer()->PostCompositorThreadCreated(
      compositor_task_runner_.get());
}

void RenderThreadImpl::InitializeWebKit(mojo::BinderMap* binders) {
  DCHECK(!blink_platform_impl_);

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

#ifdef ENABLE_VTUNE_JIT_INTERFACE
  if (command_line.HasSwitch(switches::kEnableVtune))
    gin::Debug::SetJitCodeEventHandler(vTune::GetVtuneCodeEventHandler());
#endif

  blink_platform_impl_ =
      std::make_unique<RendererBlinkPlatformImpl>(main_thread_scheduler_.get());
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
  isolate->SetAddCrashKeyCallback(AddCrashKey);

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
}

void RenderThreadImpl::InitializeRenderer(
    const std::string& user_agent,
    const blink::UserAgentMetadata& user_agent_metadata,
    const std::vector<std::string>& cors_exempt_header_list,
    network::mojom::AttributionSupport attribution_support,
    blink::mojom::OriginTrialsSettingsPtr origin_trials_settings) {
  DCHECK(user_agent_.IsNull());

  user_agent_ = WebString::FromUTF8(user_agent);
  GetContentClient()->renderer()->DidSetUserAgent(user_agent);
  user_agent_metadata_ = user_agent_metadata;
  cors_exempt_header_list_ = cors_exempt_header_list;
  attribution_support_ = attribution_support;

  blink::WebVector<blink::WebString> web_cors_exempt_header_list(
      cors_exempt_header_list.size());
  base::ranges::transform(
      cors_exempt_header_list, web_cors_exempt_header_list.begin(),
      [](const auto& header) { return blink::WebString::FromLatin1(header); });
  blink::SetCorsExemptHeaderList(web_cors_exempt_header_list);

  // In single process mode, the settings have already been set by the browser.
  if (!IsSingleProcess()) {
    blink::OriginTrialsSettingsProvider::Get()->SetSettings(
        std::move(origin_trials_settings));
  }
}

void RenderThreadImpl::RegisterSchemes() {
  // chrome:
  WebString chrome_scheme(WebString::FromASCII(kChromeUIScheme));
  WebSecurityPolicy::RegisterURLSchemeAsDisplayIsolated(chrome_scheme);
  WebSecurityPolicy::RegisterURLSchemeAsNotAllowingJavascriptURLs(
      chrome_scheme);
  WebSecurityPolicy::RegisterURLSchemeAsWebUI(chrome_scheme);

  // Service workers for chrome://
  if (base::FeatureList::IsEnabled(
          features::kEnableServiceWorkersForChromeScheme)) {
    WebSecurityPolicy::RegisterURLSchemeAsAllowingServiceWorkers(chrome_scheme);
  }

  WebString chrome_untrusted_scheme(
      WebString::FromASCII(kChromeUIUntrustedScheme));

  // chrome-untrusted:
  // Service workers for chrome-untrusted://
  if (base::FeatureList::IsEnabled(
          features::kEnableServiceWorkersForChromeUntrusted)) {
    WebSecurityPolicy::RegisterURLSchemeAsAllowingServiceWorkers(
        chrome_untrusted_scheme);
  }
  WebSecurityPolicy::RegisterURLSchemeAsNotAllowingJavascriptURLs(
      chrome_untrusted_scheme);
  WebSecurityPolicy::RegisterURLSchemeAsSupportingFetchAPI(
      chrome_untrusted_scheme);
  WebSecurityPolicy::RegisterURLSchemeAsAllowingWasmEvalCSP(
      chrome_untrusted_scheme);

  if (base::FeatureList::IsEnabled(features::kWebUICodeCache)) {
    WebSecurityPolicy::RegisterURLSchemeAsCodeCacheWithHashing(chrome_scheme);
    WebSecurityPolicy::RegisterURLSchemeAsCodeCacheWithHashing(
        chrome_untrusted_scheme);
  }

  // devtools:
  WebString devtools_scheme(WebString::FromASCII(kChromeDevToolsScheme));
  WebSecurityPolicy::RegisterURLSchemeAsDisplayIsolated(devtools_scheme);
  WebSecurityPolicy::RegisterURLSchemeAsSupportingFetchAPI(devtools_scheme);
  WebSecurityPolicy::RegisterURLSchemeAsNotAllowingJavascriptURLs(
      devtools_scheme);

  // view-source:
  WebString view_source_scheme(WebString::FromASCII(kViewSourceScheme));
  WebSecurityPolicy::RegisterURLSchemeAsDisplayIsolated(view_source_scheme);

  // chrome-error:
  WebString error_scheme(WebString::FromASCII(kChromeErrorScheme));
  WebSecurityPolicy::RegisterURLSchemeAsDisplayIsolated(error_scheme);
  WebSecurityPolicy::RegisterURLSchemeAsNotAllowingJavascriptURLs(error_scheme);
  WebSecurityPolicy::RegisterURLSchemeAsError(error_scheme);

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

int RenderThreadImpl::PostTaskToAllWebWorkers(base::RepeatingClosure closure) {
  return WorkerThreadRegistry::Instance()->PostTaskToAllThreads(
      std::move(closure));
}

media::GpuVideoAcceleratorFactories* RenderThreadImpl::GetGpuFactories() {
  DCHECK(IsMainThread());

  if (!gpu_factories_.empty()) {
    if (!gpu_factories_.back()->CheckContextProviderLostOnMainThread())
      return gpu_factories_.back().get();

    GetMediaSequencedTaskRunner()->PostTask(
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
      CreateOffscreenContext(gpu_channel_host, limits, support_locking,
                             support_gles2_interface, support_raster_interface,
                             support_oop_rasterization, support_grcontext,
                             automatic_flushes,
                             viz::command_buffer_metrics::ContextType::MEDIA,
                             kGpuStreamIdMedia, kGpuStreamPriorityMedia);

  const bool enable_video_decode_accelerator =
#if BUILDFLAG(IS_LINUX)
      base::FeatureList::IsEnabled(media::kVaapiVideoDecodeLinux) &&
#endif  // BUILDFLAG(IS_LINUX)
      !cmd_line->HasSwitch(switches::kDisableAcceleratedVideoDecode) &&
      (gpu_channel_host->gpu_feature_info()
           .status_values[gpu::GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE] ==
       gpu::kGpuFeatureStatusEnabled);

  const bool enable_video_encode_accelerator =
#if BUILDFLAG(IS_LINUX)
      base::FeatureList::IsEnabled(media::kVaapiVideoEncodeLinux) &&
#else
      !cmd_line->HasSwitch(switches::kDisableAcceleratedVideoEncode) &&
#endif  // BUILDFLAG(IS_LINUX)
      (gpu_channel_host->gpu_feature_info()
           .status_values[gpu::GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE] ==
       gpu::kGpuFeatureStatusEnabled);

  const bool enable_gpu_memory_buffers =
      !is_gpu_compositing_disabled_ &&
#if !BUILDFLAG(IS_ANDROID)
      !cmd_line->HasSwitch(switches::kDisableGpuMemoryBufferVideoFrames);
#else
      cmd_line->HasSwitch(switches::kEnableGpuMemoryBufferVideoFrames);
#endif
  const bool enable_media_stream_gpu_memory_buffers =
      enable_gpu_memory_buffers &&
      base::FeatureList::IsEnabled(
          features::kWebRtcUseGpuMemoryBufferVideoFrames);
  bool enable_video_gpu_memory_buffers = enable_gpu_memory_buffers;
#if BUILDFLAG(IS_WIN)
  enable_video_gpu_memory_buffers =
      enable_video_gpu_memory_buffers &&
      (cmd_line->HasSwitch(switches::kEnableGpuMemoryBufferVideoFrames) ||
       gpu_channel_host->gpu_info().overlay_info.supports_overlays);
#endif  // BUILDFLAG(IS_WIN)

  auto codec_factory = CreateMediaCodecFactory(media_context_provider,
                                               enable_video_decode_accelerator,
                                               enable_video_encode_accelerator);
  gpu_factories_.push_back(GpuVideoAcceleratorFactoriesImpl::Create(
      std::move(gpu_channel_host),
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      GetMediaSequencedTaskRunner(), std::move(media_context_provider),
      std::move(codec_factory), enable_video_gpu_memory_buffers,
      enable_media_stream_gpu_memory_buffers, enable_video_decode_accelerator,
      enable_video_encode_accelerator));

  gpu_factories_.back()->SetRenderingColorSpace(rendering_color_space_);
  return gpu_factories_.back().get();
}

scoped_refptr<viz::RasterContextProvider>
RenderThreadImpl::GetVideoFrameCompositorContextProvider(
    scoped_refptr<viz::RasterContextProvider> unwanted_context_provider) {
  auto video_frame_compositor_task_runner =
      blink_platform_impl_->VideoFrameCompositorTaskRunner();
  DCHECK(video_frame_compositor_task_runner);
  if (video_frame_compositor_context_provider_ &&
      video_frame_compositor_context_provider_ != unwanted_context_provider) {
    return video_frame_compositor_context_provider_;
  }

  // Need to make sure these references are removed on the correct task runner;
  if (video_frame_compositor_context_provider_) {
    video_frame_compositor_task_runner->ReleaseSoon(
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
      gpu_channel_host, limits, support_locking, support_gles2_interface,
      support_raster_interface, support_oop_rasterization, support_grcontext,
      automatic_flushes,
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

  if (is_context_result_fatal_) {
    return nullptr;
  }

  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host(
      EstablishGpuChannelSync());
  if (!gpu_channel_host) {
    shared_main_thread_contexts_ = nullptr;
    return nullptr;
  }

  bool support_locking = false;
  bool support_raster_interface = true;
  bool support_oop_rasterization =
      gpu_channel_host->gpu_feature_info()
          .status_values[gpu::GPU_FEATURE_TYPE_CANVAS_OOP_RASTERIZATION] ==
      gpu::kGpuFeatureStatusEnabled;
  bool support_gles2_interface = false;
  bool support_grcontext = !support_oop_rasterization;
  // Enable automatic flushes to improve canvas throughput.
  // See https://crbug.com/880901
  bool automatic_flushes = true;

  // We use kGpuStreamIdDefault here, the same as in
  // PepperVideoDecodeContextProvider, so we don't need to handle
  // synchronization between the pepper context and the shared main thread
  // context.
  shared_main_thread_contexts_ = CreateOffscreenContext(
      std::move(gpu_channel_host), gpu::SharedMemoryLimits(), support_locking,
      support_gles2_interface, support_raster_interface,
      support_oop_rasterization, support_grcontext, automatic_flushes,
      viz::command_buffer_metrics::ContextType::RENDERER_MAIN_THREAD,
      kGpuStreamIdDefault, kGpuStreamPriorityDefault);
  auto result = shared_main_thread_contexts_->BindToCurrentSequence();
  if (result != gpu::ContextResult::kSuccess) {
    shared_main_thread_contexts_ = nullptr;
    is_context_result_fatal_ = result == gpu::ContextResult::kFatalFailure;
  }

  return shared_main_thread_contexts_;
}

scoped_refptr<viz::ContextProviderCommandBuffer>
RenderThreadImpl::PepperVideoDecodeContextProvider() {
  DCHECK(IsMainThread());
  if (pepper_video_decode_contexts_ &&
      pepper_video_decode_contexts_->ContextGL()->GetGraphicsResetStatusKHR() ==
          GL_NO_ERROR)
    return pepper_video_decode_contexts_;

  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host(
      EstablishGpuChannelSync());
  if (!gpu_channel_host) {
    pepper_video_decode_contexts_ = nullptr;
    return nullptr;
  }

  bool support_locking = false;
  bool support_raster_interface = false;
  bool support_oop_rasterization = false;
  bool support_gles2_interface = true;
  bool support_grcontext = !support_oop_rasterization;
  bool automatic_flushes = false;
  // We use kGpuStreamIdDefault here, the same as in
  // SharedMainThreadContextProvider, so we don't need to handle
  // synchronization between the pepper context and the shared main thread
  // context.
  pepper_video_decode_contexts_ = CreateOffscreenContext(
      std::move(gpu_channel_host), gpu::SharedMemoryLimits::ForMailboxContext(),
      support_locking, support_gles2_interface, support_raster_interface,
      support_oop_rasterization, support_grcontext, automatic_flushes,
      viz::command_buffer_metrics::ContextType::RENDERER_MAIN_THREAD,
      kGpuStreamIdDefault, kGpuStreamPriorityDefault);
  auto result = pepper_video_decode_contexts_->BindToCurrentSequence();
  if (result != gpu::ContextResult::kSuccess)
    pepper_video_decode_contexts_ = nullptr;
  return pepper_video_decode_contexts_;
}

#if BUILDFLAG(IS_ANDROID)
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
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
scoped_refptr<DCOMPTextureFactory> RenderThreadImpl::GetDCOMPTextureFactory() {
  DCHECK(IsMainThread());
  if (!dcomp_texture_factory_.get() || dcomp_texture_factory_->IsLost()) {
    scoped_refptr<gpu::GpuChannelHost> channel = EstablishGpuChannelSync();
    if (!channel) {
      dcomp_texture_factory_ = nullptr;
      return nullptr;
    }
    dcomp_texture_factory_ = DCOMPTextureFactory::Create(
        std::move(channel), GetMediaSequencedTaskRunner());
  }
  return dcomp_texture_factory_;
}

OverlayStateServiceProvider*
RenderThreadImpl::GetOverlayStateServiceProvider() {
  DCHECK(IsMainThread());
  // Only set 'overlay_state_service_provider_' if Media Foundation for clear
  // is enabled.
  if (media::SupportMediaFoundationClearPlayback()) {
    if (!overlay_state_service_provider_ ||
        overlay_state_service_provider_->IsLost()) {
      scoped_refptr<gpu::GpuChannelHost> channel = EstablishGpuChannelSync();
      if (!channel) {
        overlay_state_service_provider_ = nullptr;
        return nullptr;
      }
      overlay_state_service_provider_ =
          std::make_unique<OverlayStateServiceProviderImpl>(std::move(channel));
    }
  }

  return overlay_state_service_provider_.get();
}
#endif  // BUILDFLAG(IS_WIN)

base::WaitableEvent* RenderThreadImpl::GetShutdownEvent() {
  return ChildProcess::current()->GetShutDownEvent();
}

int32_t RenderThreadImpl::GetClientId() {
  return client_id_;
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

void RenderThreadImpl::WriteIntoTrace(
    perfetto::TracedProto<perfetto::protos::pbzero::RenderProcessHost> proto) {
  int id = GetClientId();
  proto->set_id(id);
}

void RenderThreadImpl::OnAssociatedInterfaceRequest(
    const std::string& name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  if (!associated_interfaces_.TryBindInterface(name, &handle))
    ChildThreadImpl::OnAssociatedInterfaceRequest(name, std::move(handle));
}

bool RenderThreadImpl::IsLcdTextEnabled() {
  return is_lcd_text_enabled_;
}

bool RenderThreadImpl::IsElasticOverscrollEnabled() {
  return is_elastic_overscroll_enabled_;
}

gpu::GpuMemoryBufferManager* RenderThreadImpl::GetGpuMemoryBufferManager() {
  return gpu_->gpu_memory_buffer_manager();
}

blink::scheduler::WebThreadScheduler*
RenderThreadImpl::GetWebMainThreadScheduler() {
  return main_thread_scheduler_.get();
}

bool RenderThreadImpl::IsThreadedAnimationEnabled() {
  return is_threaded_animation_enabled_;
}

bool RenderThreadImpl::IsScrollAnimatorEnabled() {
  return is_scroll_animator_enabled_;
}

void RenderThreadImpl::SetScrollAnimatorEnabled(
    bool enable_scroll_animator,
    base::PassKey<AgentSchedulingGroup>) {
  is_scroll_animator_enabled_ = enable_scroll_animator;
}

bool RenderThreadImpl::IsMainThread() {
  return !!current();
}

void RenderThreadImpl::OnChannelError() {
  // In single-process mode, the renderer can't be restarted after shutdown.
  // So, if we get a channel error, crash the whole process right now to get a
  // more informative stack, since we will otherwise just crash later when we
  // try to restart it.
  CHECK(!IsSingleProcess());
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
    bool is_visible =
        visible_state == mojom::RenderProcessVisibleState::kVisible;

    if (!IsInBrowserProcess()) {
      ProcessVisibilityTracker::GetInstance()->OnProcessVisibilityChanged(
          is_visible);
    }

    if (is_visible)
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

void RenderThreadImpl::SetIsIsolatedContext(bool value) {
  blink::SetIsIsolatedContext(value);
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

void RenderThreadImpl::EstablishGpuChannel(
    EstablishGpuChannelCallback callback) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
      "gpu", "RenderThreadImpl::EstablishGpuChannel", this);
  gpu_->EstablishGpuChannel(base::BindOnce(
      [](EstablishGpuChannelCallback callback, RenderThreadImpl* thread,
         scoped_refptr<gpu::GpuChannelHost> host) {
        TRACE_EVENT_NESTABLE_ASYNC_END0(
            "gpu", "RenderThreadImpl::EstablishGpuChannel", thread);
        if (host)
          GetContentClient()->SetGpuInfo(host->gpu_info());
        std::move(callback).Run(std::move(host));
      },
      // The GPU process can crash; in that case, run the callback with no host
      // to signal the compositor to wait and try again.
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback), nullptr),
      this));
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

void RenderThreadImpl::CreateAgentSchedulingGroup(
    mojo::PendingReceiver<IPC::mojom::ChannelBootstrap> bootstrap,
    mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker> broker_remote) {
  agent_scheduling_groups_.emplace(std::make_unique<AgentSchedulingGroup>(
      *this, std::move(bootstrap), std::move(broker_remote)));
}

void RenderThreadImpl::CreateAssociatedAgentSchedulingGroup(
    mojo::PendingAssociatedReceiver<mojom::AgentSchedulingGroup>
        agent_scheduling_group,
    mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker> broker_remote) {
  agent_scheduling_groups_.emplace(std::make_unique<AgentSchedulingGroup>(
      *this, std::move(agent_scheduling_group), std::move(broker_remote)));
}

void RenderThreadImpl::OnNetworkConnectionChanged(
    net::NetworkChangeNotifier::ConnectionType type,
    double max_bandwidth_mbps) {
  bool online_status = type != net::NetworkChangeNotifier::CONNECTION_NONE;
  WebNetworkStateNotifier::SetOnLine(online_status);
  WebNetworkStateNotifier::SetWebConnection(
      NetConnectionTypeToWebConnectionType(type), max_bandwidth_mbps);
  if (url_loader_throttle_provider_)
    url_loader_throttle_provider_->SetOnline(online_status);
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
#if BUILDFLAG(IS_ANDROID)
  if (suspend) {
    main_thread_scheduler_->PauseTimersForAndroidWebView();
  } else {
    main_thread_scheduler_->ResumeTimersForAndroidWebView();
  }
#else
  NOTREACHED();
#endif
}

void RenderThreadImpl::UpdateScrollbarTheme(
    mojom::UpdateScrollbarThemeParamsPtr params) {
#if BUILDFLAG(IS_MAC)
  blink::WebScrollbarTheme::UpdateScrollbarsWithNSDefaults(
      params->has_initial_button_delay
          ? absl::make_optional(params->initial_button_delay)
          : absl::nullopt,
      params->has_autoscroll_button_delay
          ? absl::make_optional(params->autoscroll_button_delay)
          : absl::nullopt,
      params->preferred_scroller_style, params->redraw,
      params->jump_on_track_click);
#endif  // BUILDFLAG(IS_MAC)
#if BUILDFLAG(IS_APPLE)
  is_elastic_overscroll_enabled_ = params->scroll_view_rubber_banding;
#else
  NOTREACHED();
#endif  // BUILDFLAG(IS_APPLE)
}

void RenderThreadImpl::OnSystemColorsChanged(int32_t aqua_color_variant) {
#if BUILDFLAG(IS_MAC)
  SystemColorsDidChange(aqua_color_variant);

  // Let blink know it should invalidate and recalculate styles for elements
  // that rely on system colors, such as the accent and highlight colors.
  blink::SystemColorsChanged();
#else
  NOTREACHED();
#endif
}

void RenderThreadImpl::UpdateSystemColorInfo(
    mojom::UpdateSystemColorInfoParamsPtr params) {
  if (blink_platform_impl_->ThemeEngine()->UpdateColorProviders(
          params->light_colors, params->dark_colors)) {
    // Notify blink that the global ColorProvider instances for this renderer
    // have changed. These color providers are only used to paint native
    // controls and only require us to invalidate paint for local frames in this
    // renderer.
    blink::ColorProvidersChanged();
  }

  bool did_system_color_info_change =
      ui::NativeTheme::GetInstanceForWeb()->UpdateSystemColorInfo(
          params->is_dark_mode, params->forced_colors, params->colors);

  if (did_system_color_info_change) {
    // Notify blink of system color info changes. These give blink the
    // opportunity to update internal state to reflect the NativeTheme's color
    // scheme. These will also prompt blink to invalidate and recalculate styles
    // for elements that rely on system colors, such as those leveraging the
    // forced colors media feature. These can affect CSS styles and thus require
    // action beyond simply invalidating paint on local frames.
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

void RenderThreadImpl::PurgeResourceCache(PurgeResourceCacheCallback callback) {
  blink::WebCache::Clear();
  std::move(callback).Run();
}

void RenderThreadImpl::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  TRACE_EVENT(
      "memory", "RenderThreadImpl::OnMemoryPressure",
      [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_chrome_memory_pressure_notification();
        data->set_level(base::trace_event::MemoryPressureLevelToTraceEnum(
            memory_pressure_level));
      });
  if (blink_platform_impl_)
    blink::WebMemoryPressureListener::OnMemoryPressure(memory_pressure_level);
  if (memory_pressure_level ==
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    ReleaseFreeMemory();
  }
}

scoped_refptr<base::SequencedTaskRunner>
RenderThreadImpl::GetMediaSequencedTaskRunner() {
  DCHECK(main_thread_runner()->BelongsToCurrentThread());
  if (base::FeatureList::IsEnabled(kUseThreadPoolForMediaTaskRunner)) {
    if (!media_task_runner_) {
      media_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
          base::TaskTraits{base::TaskPriority::USER_VISIBLE,
                           base::WithBaseSyncPrimitives(), base::MayBlock()});
    }
    return media_task_runner_;
  }
  if (!media_thread_) {
    media_thread_ = std::make_unique<base::Thread>("Media");
#if BUILDFLAG(IS_FUCHSIA)
    // Start IO thread on Fuchsia to make that thread usable for FIDL.
    base::Thread::Options options(base::MessagePumpType::IO, 0);
    // TODO(crbug.com/1400772): Use kCompositing to address media latency on
    // Fuchsia until alignment on new media thread types is achieved.
    options.thread_type = base::ThreadType::kCompositing;
#else
    base::Thread::Options options;
#endif
    media_thread_->StartWithOptions(std::move(options));
  }
  return media_thread_->task_runner();
}

scoped_refptr<cc::RasterContextProviderWrapper>
RenderThreadImpl::SharedCompositorWorkerContextProvider(
    cc::RasterDarkModeFilter* dark_mode_filter) {
  DCHECK(IsMainThread());
  // Try to reuse existing shared worker context provider.
  if (shared_worker_context_provider_wrapper_) {
    // Note: If context is lost, delete reference after releasing the lock.
    viz::RasterContextProvider::ScopedRasterContextLock lock(
        shared_worker_context_provider_wrapper_->GetContext().get());
    if (lock.RasterInterface()->GetGraphicsResetStatusKHR() == GL_NO_ERROR)
      return shared_worker_context_provider_wrapper_;
  }

  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host(
      EstablishGpuChannelSync());
  if (!gpu_channel_host) {
    shared_worker_context_provider_wrapper_ = nullptr;
    return shared_worker_context_provider_wrapper_;
  }

  bool support_locking = true;
  bool support_gpu_rasterization =
      gpu_channel_host->gpu_feature_info()
          .status_values[gpu::GPU_FEATURE_TYPE_GPU_RASTERIZATION] ==
      gpu::kGpuFeatureStatusEnabled;

  bool support_gles2_interface = false;
  bool support_raster_interface = true;
  bool support_grcontext = false;
  bool automatic_flushes = false;
  auto shared_memory_limits =
      support_gpu_rasterization ? gpu::SharedMemoryLimits::ForOOPRasterContext()
                                : gpu::SharedMemoryLimits();
  scoped_refptr<viz::ContextProviderCommandBuffer>
      shared_worker_context_provider = CreateOffscreenContext(
          std::move(gpu_channel_host), shared_memory_limits, support_locking,
          support_gles2_interface, support_raster_interface,
          support_gpu_rasterization, support_grcontext, automatic_flushes,
          viz::command_buffer_metrics::ContextType::RENDER_WORKER,
          kGpuStreamIdWorker, kGpuStreamPriorityWorker);

  auto result = shared_worker_context_provider->BindToCurrentSequence();
  if (result != gpu::ContextResult::kSuccess)
    return nullptr;

  shared_worker_context_provider_wrapper_ =
      base::MakeRefCounted<cc::RasterContextProviderWrapper>(
          std::move(shared_worker_context_provider), dark_mode_filter,
          cc::ImageDecodeCacheUtils::GetWorkingSetBytesForImageDecode(
              /*for_renderer=*/true));

  return shared_worker_context_provider_wrapper_;
}

bool RenderThreadImpl::RendererIsHidden() const {
  return visible_state_ == mojom::RenderProcessVisibleState::kHidden;
}

void RenderThreadImpl::OnRendererHidden() {
  blink::IsolateInBackgroundNotification();

  // TODO(rmcilroy): Remove IdleHandler and replace it with an IdleTask
  // scheduled by the RendererScheduler - http://crbug.com/469210.
  if (!GetContentClient()->renderer()->RunIdleHandlerWhenWidgetsHidden())
    return;
  main_thread_scheduler_->SetRendererHidden(true);
}

void RenderThreadImpl::OnRendererVisible() {
  blink::IsolateInForegroundNotification();

  if (!GetContentClient()->renderer()->RunIdleHandlerWhenWidgetsHidden())
    return;
  main_thread_scheduler_->SetRendererHidden(false);
}

bool RenderThreadImpl::RendererIsBackgrounded() const {
  return background_state_ ==
         mojom::RenderProcessBackgroundState::kBackgrounded;
}

void RenderThreadImpl::OnRendererBackgrounded() {
  UpdateForegroundCrashKey(/*foreground=*/false);
  main_thread_scheduler_->SetRendererBackgrounded(true);
  discardable_memory_allocator_->OnBackgrounded();
  base::allocator::PartitionAllocSupport::Get()->OnBackgrounded();
}

void RenderThreadImpl::OnRendererForegrounded() {
  UpdateForegroundCrashKey(/*foreground=*/true);
  main_thread_scheduler_->SetRendererBackgrounded(false);
  discardable_memory_allocator_->OnForegrounded();
  base::allocator::PartitionAllocSupport::Get()->OnForegrounded(
      MainFrameCounter::has_main_frame());
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

void RenderThreadImpl::OnRendererInterfaceReceiver(
    mojo::PendingAssociatedReceiver<mojom::Renderer> receiver) {
  DCHECK(!renderer_receiver_.is_bound());
  renderer_receiver_.Bind(
      std::move(receiver),
      GetWebMainThreadScheduler()->DeprecatedDefaultTaskRunner());
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

gfx::ColorSpace RenderThreadImpl::GetRenderingColorSpace() {
  DCHECK(IsMainThread());
  return rendering_color_space_;
}

network::mojom::AttributionSupport
RenderThreadImpl::GetAttributionReportingSupport() {
  return attribution_support_;
}

void RenderThreadImpl::SetAttributionReportingSupport(
    network::mojom::AttributionSupport attribution_support) {
  attribution_support_ = attribution_support;
}

std::unique_ptr<CodecFactory> RenderThreadImpl::CreateMediaCodecFactory(
    scoped_refptr<viz::ContextProviderCommandBuffer> context_provider,
    bool enable_video_decode_accelerator,
    bool enable_video_encode_accelerator) {
  mojo::PendingRemote<media::mojom::VideoEncodeAcceleratorProvider>
      vea_provider;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  if (base::FeatureList::IsEnabled(media::kUseOutOfProcessVideoEncoding)) {
    BindHostReceiver(vea_provider.InitWithNewPipeAndPassReceiver());
  } else {
    gpu_->CreateVideoEncodeAcceleratorProvider(
        vea_provider.InitWithNewPipeAndPassReceiver());
  }
#else
  gpu_->CreateVideoEncodeAcceleratorProvider(
      vea_provider.InitWithNewPipeAndPassReceiver());
#endif

#if BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)
  mojo::PendingRemote<media::mojom::InterfaceFactory> interface_factory;
  BindHostReceiver(interface_factory.InitWithNewPipeAndPassReceiver());
  return std::make_unique<CodecFactoryMojo>(
      GetMediaSequencedTaskRunner(), context_provider,
      enable_video_decode_accelerator, enable_video_encode_accelerator,
      std::move(vea_provider), std::move(interface_factory));
#elif BUILDFLAG(IS_FUCHSIA)
  mojo::PendingRemote<media::mojom::FuchsiaMediaCodecProvider>
      media_codec_provider;
  BindHostReceiver(media_codec_provider.InitWithNewPipeAndPassReceiver());
  return std::make_unique<CodecFactoryFuchsia>(
      GetMediaSequencedTaskRunner(), context_provider,
      enable_video_decode_accelerator, enable_video_encode_accelerator,
      std::move(vea_provider), std::move(media_codec_provider));
#else
  return std::make_unique<CodecFactoryDefault>(
      GetMediaSequencedTaskRunner(), context_provider,
      enable_video_decode_accelerator, enable_video_encode_accelerator,
      std::move(vea_provider));
#endif
}

#if BUILDFLAG(IS_ANDROID)
void RenderThreadImpl::SetPrivateMemoryFootprint(
    uint64_t private_memory_footprint_bytes) {
  GetRendererHost()->SetPrivateMemoryFootprint(private_memory_footprint_bytes);
}

void RenderThreadImpl::OnMemoryPressureFromBrowserReceived(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  // To avoid that the browser process requests a signal while a renderer
  // is creating and blink has not been initialized yet, check
  // |blink_platform_impl_| here.
  if (!blink_platform_impl_) {
    return;
  }
  blink::RequestUserLevelMemoryPressureSignal();
}

#endif

}  // namespace content
