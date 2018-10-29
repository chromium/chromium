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
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/discardable_memory_allocator.h"
#include "base/memory/shared_memory.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
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
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_settings.h"
#include "components/discardable_memory/client/client_discardable_shared_memory_manager.h"
#include "components/metrics/public/interfaces/single_sample_metrics.mojom.h"
#include "components/metrics/single_sample_metrics.h"
#include "components/viz/client/hit_test_data_provider.h"
#include "components/viz/client/hit_test_data_provider_draw_quad.h"
#include "components/viz/client/local_surface_id_provider.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/switches.h"
#include "content/child/runtime_features.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/buildflags.h"
#include "content/common/content_constants_internal.h"
#include "content/common/dom_storage/dom_storage_messages.h"
#include "content/common/frame_messages.h"
#include "content/common/frame_owner_properties.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/gpu_stream_constants.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/resource_usage_reporter.mojom.h"
#include "content/public/common/resource_usage_reporter_type_converters.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/simple_connection_filter.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/render_thread_observer.h"
#include "content/public/renderer/render_view_visitor.h"
#include "content/renderer/appcache/appcache_dispatcher.h"
#include "content/renderer/appcache/appcache_frontend_impl.h"
#include "content/renderer/browser_plugin/browser_plugin_manager.h"
#include "content/renderer/categorized_worker_pool.h"
#include "content/renderer/dom_storage/dom_storage_dispatcher.h"
#include "content/renderer/dom_storage/webstoragearea_impl.h"
#include "content/renderer/dom_storage/webstoragenamespace_impl.h"
#include "content/renderer/effective_connection_type_helper.h"
#include "content/renderer/gpu/frame_swap_message_queue.h"
#include "content/renderer/input/widget_input_handler_manager.h"
#include "content/renderer/loader/resource_dispatcher.h"
#include "content/renderer/low_memory_mode_controller.h"
#include "content/renderer/media/audio/audio_renderer_mixer_manager.h"
#include "content/renderer/media/gpu/gpu_video_accelerator_factories_impl.h"
#include "content/renderer/media/midi/midi_message_filter.h"
#include "content/renderer/media/render_media_client.h"
#include "content/renderer/media/stream/aec_dump_message_filter.h"
#include "content/renderer/media/stream/media_stream_center.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "content/renderer/media/webrtc/peer_connection_dependency_factory.h"
#include "content/renderer/media/webrtc/peer_connection_tracker.h"
#include "content/renderer/media/webrtc/rtc_peer_connection_handler.h"
#include "content/renderer/mus/render_widget_window_tree_client_factory.h"
#include "content/renderer/mus/renderer_window_tree_client.h"
#include "content/renderer/net_info_helper.h"
#include "content/renderer/p2p/socket_dispatcher.h"
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/render_process_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/renderer_blink_platform_impl.h"
#include "content/renderer/service_worker/embedded_worker_instance_client_impl.h"
#include "content/renderer/service_worker/service_worker_context_client.h"
#include "content/renderer/shared_worker/embedded_shared_worker_stub.h"
#include "content/renderer/shared_worker/shared_worker_factory_impl.h"
#include "content/renderer/web_database_observer_impl.h"
#include "content/renderer/worker_thread_registry.h"
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
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "net/base/net_errors.h"
#include "net/base/port_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/ws/public/cpp/gpu/context_provider_command_buffer.h"
#include "services/ws/public/cpp/gpu/gpu.h"
#include "services/ws/public/mojom/constants.mojom.h"
#include "skia/ext/skia_memory_dump_provider.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/platform/web_cache.h"
#include "third_party/blink/public/platform/web_image_generator.h"
#include "third_party/blink/public/platform/web_memory_coordinator.h"
#include "third_party/blink/public/platform/web_network_state_notifier.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame.h"
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
#include "content/renderer/android/synchronous_layer_tree_frame_sink.h"
#include "content/renderer/media/android/stream_texture_factory.h"
#include "media/base/android/media_codec_util.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "content/renderer/theme_helper_mac.h"
#endif

#if defined(OS_WIN)
#include <windows.h>
#include <objbase.h>
#endif

#ifdef ENABLE_VTUNE_JIT_INTERFACE
#include "v8/src/third_party/vtune/v8-vtune.h"
#endif

#if defined(ENABLE_IPC_FUZZER)
#include "content/common/external_ipc_dumper.h"
#include "mojo/public/cpp/bindings/message_dumper.h"
#endif

#if defined(OS_MACOSX)
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif

using base::ThreadRestrictions;
using blink::WebDocument;
using blink::WebFrame;
using blink::WebNetworkStateNotifier;
using blink::WebRuntimeFeatures;
using blink::WebScriptController;
using blink::WebSecurityPolicy;
using blink::WebString;
using blink::WebView;

namespace content {

namespace {

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
        v8::MemoryPressureLevel::kNone, "none level not align");
static_assert(static_cast<v8::MemoryPressureLevel>(
    base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE) ==
        v8::MemoryPressureLevel::kModerate, "moderate level not align");
static_assert(static_cast<v8::MemoryPressureLevel>(
    base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) ==
        v8::MemoryPressureLevel::kCritical, "critical level not align");

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

void* CreateHistogram(
    const char *name, int min, int max, size_t buckets) {
  if (min <= 0)
    min = 1;
  std::string histogram_name;
  RenderThreadImpl* render_thread_impl = RenderThreadImpl::current();
  if (render_thread_impl) {  // Can be null in tests.
    histogram_name = render_thread_impl->
        histogram_customizer()->ConvertToCustomHistogramName(name);
  } else {
    histogram_name = std::string(name);
  }
  base::HistogramBase* histogram = base::Histogram::FactoryGet(
      histogram_name, min, max, buckets,
      base::Histogram::kUmaTargetedHistogramFlag);
  return histogram;
}

void AddHistogramSample(void* hist, int sample) {
  base::Histogram* histogram = static_cast<base::Histogram*>(hist);
  histogram->Add(sample);
}

class FrameFactoryImpl : public mojom::FrameFactory {
 public:
  explicit FrameFactoryImpl(const service_manager::BindSourceInfo& source_info)
      : source_info_(source_info), routing_id_highmark_(-1) {}

 private:
  // mojom::FrameFactory:
  void CreateFrame(int32_t frame_routing_id,
                   mojom::FrameRequest frame_request) override {
    // TODO(morrita): This is for investigating http://crbug.com/415059 and
    // should be removed once it is fixed.
    CHECK_LT(routing_id_highmark_, frame_routing_id);
    routing_id_highmark_ = frame_routing_id;

    RenderFrameImpl* frame = RenderFrameImpl::FromRoutingID(frame_routing_id);
    // We can receive a GetServiceProviderForFrame message for a frame not yet
    // created due to a race between the message and a
    // mojom::Renderer::CreateView IPC that triggers creation of the RenderFrame
    // we want.
    if (!frame) {
      RenderThreadImpl::current()->RegisterPendingFrameCreate(
          source_info_, frame_routing_id, std::move(frame_request));
      return;
    }

    frame->BindFrame(source_info_, std::move(frame_request));
  }

 private:
  service_manager::BindSourceInfo source_info_;
  int32_t routing_id_highmark_;
};

void CreateFrameFactory(mojom::FrameFactoryRequest request,
                        const service_manager::BindSourceInfo& source_info) {
  mojo::MakeStrongBinding(std::make_unique<FrameFactoryImpl>(source_info),
                          std::move(request));
}

scoped_refptr<ws::ContextProviderCommandBuffer> CreateOffscreenContext(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    const gpu::SharedMemoryLimits& limits,
    bool support_locking,
    bool support_gles2_interface,
    bool support_raster_interface,
    bool support_oop_rasterization,
    bool support_grcontext,
    bool automatic_flushes,
    ws::command_buffer_metrics::ContextType type,
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
  // Using RasterDecoder for OOP-R backend, so we need support_raster_interface
  // and !support_gles2_interface.
  attributes.enable_oop_rasterization = support_oop_rasterization &&
                                        support_raster_interface &&
                                        !support_gles2_interface;
  return base::MakeRefCounted<ws::ContextProviderCommandBuffer>(
      std::move(gpu_channel_host), gpu_memory_buffer_manager, stream_id,
      stream_priority, gpu::kNullSurfaceHandle,
      GURL("chrome://gpu/RenderThreadImpl::CreateOffscreenContext/" +
           ws::command_buffer_metrics::ContextTypeToString(type)),
      automatic_flushes, support_locking, support_grcontext, limits, attributes,
      type);
}

// Hook that allows single-sample metric code from //components/metrics to
// connect from the renderer process to the browser process.
void CreateSingleSampleMetricsProvider(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    service_manager::Connector* connector,
    metrics::mojom::SingleSampleMetricsProviderRequest request) {
  if (task_runner->BelongsToCurrentThread()) {
    connector->BindInterface(mojom::kBrowserServiceName, std::move(request));
    return;
  }

  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&CreateSingleSampleMetricsProvider, std::move(task_runner),
                     connector, std::move(request)));
}

class RendererLocalSurfaceIdProvider : public viz::LocalSurfaceIdProvider {
 public:
  const viz::LocalSurfaceId& GetLocalSurfaceIdForFrame(
      const viz::CompositorFrame& frame) override {
    auto new_surface_properties =
        RenderWidgetSurfaceProperties::FromCompositorFrame(frame);
    if (new_surface_properties != surface_properties_) {
      parent_local_surface_id_allocator_.GenerateId();
      surface_properties_ = new_surface_properties;
    }
    return parent_local_surface_id_allocator_.GetCurrentLocalSurfaceId();
  }

 private:
  RenderWidgetSurfaceProperties surface_properties_;
};

// This factory is used to defer binding of the InterfacePtr to the compositor
// thread.
class UkmRecorderFactoryImpl : public cc::UkmRecorderFactory {
 public:
  explicit UkmRecorderFactoryImpl(
      std::unique_ptr<service_manager::Connector> connector)
      : connector_(std::move(connector)) {
    DCHECK(connector_);
  }
  ~UkmRecorderFactoryImpl() override = default;

  std::unique_ptr<ukm::UkmRecorder> CreateRecorder() override {
    return ukm::MojoUkmRecorder::Create(connector_.get());
  }

 private:
  std::unique_ptr<service_manager::Connector> connector_;
};

static const int kWaitForWorkersStatsTimeoutMS = 20;

class ResourceUsageReporterImpl : public content::mojom::ResourceUsageReporter {
 public:
  explicit ResourceUsageReporterImpl(base::WeakPtr<RenderThread> thread)
      : workers_to_go_(0), thread_(thread), weak_factory_(this) {}
  ~ResourceUsageReporterImpl() override {}

 private:
  static void CollectOnWorkerThread(
      const scoped_refptr<base::TaskRunner>& master,
      base::WeakPtr<ResourceUsageReporterImpl> impl) {
    size_t total_bytes = 0;
    size_t used_bytes = 0;
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    if (isolate) {
      v8::HeapStatistics heap_stats;
      isolate->GetHeapStatistics(&heap_stats);
      total_bytes = heap_stats.total_heap_size();
      used_bytes = heap_stats.used_heap_size();
    }
    master->PostTask(FROM_HERE,
                     base::BindOnce(&ResourceUsageReporterImpl::ReceiveStats,
                                    impl, total_bytes, used_bytes));
  }

  void ReceiveStats(size_t total_bytes, size_t used_bytes) {
    usage_data_->v8_bytes_allocated += total_bytes;
    usage_data_->v8_bytes_used += used_bytes;
    workers_to_go_--;
    if (!workers_to_go_)
      SendResults();
  }

  void SendResults() {
    if (!callback_.is_null())
      std::move(callback_).Run(std::move(usage_data_));
    callback_.Reset();
    weak_factory_.InvalidateWeakPtrs();
    workers_to_go_ = 0;
  }

  void GetUsageData(GetUsageDataCallback callback) override {
    DCHECK(callback_.is_null());
    weak_factory_.InvalidateWeakPtrs();
    usage_data_ = mojom::ResourceUsageData::New();
    usage_data_->reports_v8_stats = true;
    callback_ = std::move(callback);

    // Since it is not safe to call any Blink or V8 functions until Blink has
    // been initialized (which also initializes V8), early out and send 0 back
    // for all resources.
    if (!thread_) {
      SendResults();
      return;
    }

    blink::WebCache::ResourceTypeStats stats;
    blink::WebCache::GetResourceTypeStats(&stats);
    usage_data_->web_cache_stats = mojom::ResourceTypeStats::From(stats);

    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    if (isolate) {
      v8::HeapStatistics heap_stats;
      isolate->GetHeapStatistics(&heap_stats);
      usage_data_->v8_bytes_allocated = heap_stats.total_heap_size();
      usage_data_->v8_bytes_used = heap_stats.used_heap_size();
    }
    base::RepeatingClosure collect = base::BindRepeating(
        &ResourceUsageReporterImpl::CollectOnWorkerThread,
        base::ThreadTaskRunnerHandle::Get(), weak_factory_.GetWeakPtr());
    workers_to_go_ =
        RenderThread::Get()->PostTaskToAllWebWorkers(std::move(collect));
    if (workers_to_go_) {
      // The guard task to send out partial stats
      // in case some workers are not responsive.
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&ResourceUsageReporterImpl::SendResults,
                         weak_factory_.GetWeakPtr()),
          base::TimeDelta::FromMilliseconds(kWaitForWorkersStatsTimeoutMS));
    } else {
      // No worker threads so just send out the main thread data right away.
      SendResults();
    }
  }

  mojom::ResourceUsageDataPtr usage_data_;
  GetUsageDataCallback callback_;
  int workers_to_go_;
  base::WeakPtr<RenderThread> thread_;

  base::WeakPtrFactory<ResourceUsageReporterImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ResourceUsageReporterImpl);
};

void CreateResourceUsageReporter(base::WeakPtr<RenderThread> thread,
                                 mojom::ResourceUsageReporterRequest request) {
  mojo::MakeStrongBinding(std::make_unique<ResourceUsageReporterImpl>(thread),
                          std::move(request));
}

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
    const std::string& host, size_t view_count) {
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
    std::unique_ptr<blink::scheduler::WebThreadScheduler> scheduler)
    : ChildThreadImpl(base::DoNothing(),
                      Options::Builder()
                          .InBrowserProcess(params)
                          .AutoStartServiceManagerConnection(false)
                          .ConnectToBrowser(true)
                          .IPCTaskRunner(scheduler->IPCTaskRunner())
                          .Build()),
      main_thread_scheduler_(std::move(scheduler)),
      categorized_worker_pool_(new CategorizedWorkerPool()),
      renderer_binding_(this),
      client_id_(1),
      compositing_mode_watcher_binding_(this),
      weak_factory_(this) {
  TRACE_EVENT0("startup", "RenderThreadImpl::Create");
  Init();
}

// Multi-process mode.
RenderThreadImpl::RenderThreadImpl(
    base::RepeatingClosure quit_closure,
    std::unique_ptr<blink::scheduler::WebThreadScheduler> scheduler)
    : ChildThreadImpl(std::move(quit_closure),
                      Options::Builder()
                          .AutoStartServiceManagerConnection(false)
                          .ConnectToBrowser(true)
                          .IPCTaskRunner(scheduler->IPCTaskRunner())
                          .Build()),
      main_thread_scheduler_(std::move(scheduler)),
      categorized_worker_pool_(new CategorizedWorkerPool()),
      is_scroll_animator_enabled_(false),
      renderer_binding_(this),
      compositing_mode_watcher_binding_(this),
      weak_factory_(this) {
  TRACE_EVENT0("startup", "RenderThreadImpl::Create");
  DCHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kRendererClientId));
  base::StringToInt(base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                        switches::kRendererClientId),
                    &client_id_);
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
  blink::WebView::SetUseExternalPopupMenus(true);
#endif

  lazy_tls.Pointer()->Set(this);
  g_main_task_runner.Get() = base::ThreadTaskRunnerHandle::Get();

  // Register this object as the main thread.
  ChildProcess::current()->set_main_thread(this);

  metrics::InitializeSingleSampleMetricsFactory(
      base::BindRepeating(&CreateSingleSampleMetricsProvider,
                          main_thread_runner(), GetConnector()));

  gpu_ = ws::Gpu::Create(GetConnector(),
                         features::IsMultiProcessMash()
                             ? ws::mojom::kServiceName
                             : mojom::kBrowserServiceName,
                         GetIOTaskRunner());

  resource_dispatcher_.reset(new ResourceDispatcher());
  url_loader_throttle_provider_ =
      GetContentClient()->renderer()->CreateURLLoaderThrottleProvider(
          URLLoaderThrottleProviderType::kFrame);

  auto registry = std::make_unique<service_manager::BinderRegistry>();
  InitializeWebKit(registry.get());

  // In single process the single process is all there is.
  widget_count_ = 0;
  hidden_widget_count_ = 0;

  appcache_dispatcher_.reset(
      new AppCacheDispatcher(new AppCacheFrontendImpl()));
  registry->AddInterface(
      base::BindRepeating(&AppCacheDispatcher::Bind,
                          base::Unretained(appcache_dispatcher())),
      GetWebMainThreadScheduler()->IPCTaskRunner());
  dom_storage_dispatcher_.reset(new DomStorageDispatcher());

  vc_manager_.reset(new VideoCaptureImplManager());

  browser_plugin_manager_.reset(new BrowserPluginManager());
  AddObserver(browser_plugin_manager_.get());

  peer_connection_tracker_.reset(
      new PeerConnectionTracker(main_thread_runner()));
  AddObserver(peer_connection_tracker_.get());

  p2p_socket_dispatcher_ = new P2PSocketDispatcher();

  peer_connection_factory_.reset(
      new PeerConnectionDependencyFactory(p2p_socket_dispatcher_.get()));

  aec_dump_message_filter_ =
      new AecDumpMessageFilter(GetIOTaskRunner(), main_thread_runner());

  AddFilter(aec_dump_message_filter_.get());

  audio_input_ipc_factory_.emplace(main_thread_runner(), GetIOTaskRunner());

  audio_output_ipc_factory_.emplace(GetIOTaskRunner());

  midi_message_filter_ = new MidiMessageFilter(GetIOTaskRunner());
  AddFilter(midi_message_filter_.get());

#if defined(USE_AURA)
  if (features::IsMultiProcessMash())
    CreateRenderWidgetWindowTreeClientFactory(GetServiceManagerConnection());
#endif

  registry->AddInterface(base::Bind(&SharedWorkerFactoryImpl::Create),
                         base::ThreadTaskRunnerHandle::Get());
  registry->AddInterface(base::BindRepeating(CreateResourceUsageReporter,
                                             weak_factory_.GetWeakPtr()),
                         base::ThreadTaskRunnerHandle::Get());

  GetServiceManagerConnection()->AddConnectionFilter(
      std::make_unique<SimpleConnectionFilter>(std::move(registry)));

  {
    auto registry_with_source_info =
        std::make_unique<service_manager::BinderRegistryWithArgs<
            const service_manager::BindSourceInfo&>>();
    registry_with_source_info->AddInterface(
        base::Bind(&CreateFrameFactory), base::ThreadTaskRunnerHandle::Get());
    GetServiceManagerConnection()->AddConnectionFilter(
        std::make_unique<SimpleConnectionFilterWithSourceInfo>(
            std::move(registry_with_source_info)));
  }

  GetContentClient()->renderer()->RenderThreadStarted();

  StartServiceManagerConnection();

  GetAssociatedInterfaceRegistry()->AddInterface(
      base::Bind(&RenderThreadImpl::OnRendererInterfaceRequest,
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

  cc::SetClientNameForMetrics("Renderer");

  is_threaded_animation_enabled_ =
      !command_line.HasSwitch(cc::switches::kDisableThreadedAnimation);

  is_zero_copy_enabled_ = command_line.HasSwitch(switches::kEnableZeroCopy);
  is_partial_raster_enabled_ =
      !command_line.HasSwitch(switches::kDisablePartialRaster);
  is_gpu_memory_buffer_compositor_resources_enabled_ = command_line.HasSwitch(
      switches::kEnableGpuMemoryBufferCompositorResources);

// On macOS this value is adjusted in `UpdateScrollbarTheme()`,
// but the system default is true.
#if defined(OS_MACOSX)
  is_elastic_overscroll_enabled_ = true;
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
#else
    is_lcd_text_enabled_ = true;
#endif
  }

  if (command_line.HasSwitch(switches::kDisableGpuCompositing))
    is_gpu_compositing_disabled_ = true;

  is_gpu_rasterization_forced_ =
      command_line.HasSwitch(switches::kForceGpuRasterization);

  if (command_line.HasSwitch(switches::kGpuRasterizationMSAASampleCount)) {
    std::string string_value = command_line.GetSwitchValueASCII(
        switches::kGpuRasterizationMSAASampleCount);
    bool parsed_msaa_sample_count =
        base::StringToInt(string_value, &gpu_rasterization_msaa_sample_count_);
    DCHECK(parsed_msaa_sample_count) << string_value;
    DCHECK_GE(gpu_rasterization_msaa_sample_count_, 0);
  } else {
    gpu_rasterization_msaa_sample_count_ = -1;
  }

  // Note that under Linux, the media library will normally already have
  // been initialized by the Zygote before this instance became a Renderer.
  media::InitializeMediaLibrary();

#if defined(OS_ANDROID)
  if (!command_line.HasSwitch(switches::kDisableAcceleratedVideoDecode) &&
      media::MediaCodecUtil::IsMediaCodecAvailable()) {
    bool accelerated_video_decode_blacklisted = false;
    if (!command_line.HasSwitch(switches::kIgnoreGpuBlacklist)) {
      int32_t major_version = 0, minor_version = 0, bugfix_version = 0;
      base::SysInfo::OperatingSystemVersionNumbers(
          &major_version, &minor_version, &bugfix_version);
      if (major_version < 5) {
        // Currently accelerated video decode is only blacklisted on
        // Android older than Lollipop.
        scoped_refptr<gpu::GpuChannelHost> gpu_channel_host =
            EstablishGpuChannelSync();
        if (!gpu_channel_host ||
            gpu_channel_host->gpu_feature_info().status_values
                    [gpu::GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE] !=
                gpu::kGpuFeatureStatusEnabled) {
          accelerated_video_decode_blacklisted = true;
        }
      }
    }
    if (!accelerated_video_decode_blacklisted)
      media::EnablePlatformDecoderSupport();
  }
#endif

  memory_pressure_listener_.reset(new base::MemoryPressureListener(
      base::Bind(&RenderThreadImpl::OnMemoryPressure, base::Unretained(this)),
      base::Bind(&RenderThreadImpl::OnSyncMemoryPressure,
                 base::Unretained(this))));

  int num_raster_threads = 0;
  std::string string_value =
      command_line.GetSwitchValueASCII(switches::kNumRasterThreads);
  bool parsed_num_raster_threads =
      base::StringToInt(string_value, &num_raster_threads);
  DCHECK(parsed_num_raster_threads) << string_value;
  DCHECK_GT(num_raster_threads, 0);

#if defined(OS_LINUX)
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

  discardable_memory::mojom::DiscardableSharedMemoryManagerPtr manager_ptr;
  if (features::IsMultiProcessMash()) {
#if defined(USE_AURA)
    GetServiceManagerConnection()->GetConnector()->BindInterface(
        ws::mojom::kServiceName, &manager_ptr);
#else
    NOTREACHED();
#endif
  } else {
    ChildThread::Get()->GetConnector()->BindInterface(
        mojom::kBrowserServiceName, mojo::MakeRequest(&manager_ptr));
  }

  discardable_shared_memory_manager_ = std::make_unique<
      discardable_memory::ClientDiscardableSharedMemoryManager>(
      std::move(manager_ptr), GetIOTaskRunner());

  // TODO(boliu): In single process, browser main loop should set up the
  // discardable memory manager, and should skip this if kSingleProcess.
  // See crbug.com/503724.
  base::DiscardableMemoryAllocator::SetInstance(
      discardable_shared_memory_manager_.get());

  GetConnector()->BindInterface(mojom::kBrowserServiceName,
                                mojo::MakeRequest(&storage_partition_service_));

#if defined(OS_LINUX)
  render_message_filter()->SetThreadPriority(
      ChildProcess::current()->io_thread_id(), base::ThreadPriority::DISPLAY);
#endif

  process_foregrounded_count_ = 0;
  needs_to_record_first_active_paint_ = false;
  was_backgrounded_time_ = base::TimeTicks::Min();

  GetConnector()->BindInterface(mojom::kBrowserServiceName,
                                mojo::MakeRequest(&frame_sink_provider_));

  if (!is_gpu_compositing_disabled_) {
    GetConnector()->BindInterface(
        mojom::kBrowserServiceName,
        mojo::MakeRequest(&compositing_mode_reporter_));

    // Make |this| a CompositingModeWatcher for the
    // |compositing_mode_reporter_|.
    viz::mojom::CompositingModeWatcherPtr watcher_ptr;
    compositing_mode_watcher_binding_.Bind(mojo::MakeRequest(&watcher_ptr));
    compositing_mode_reporter_->AddCompositingModeWatcher(
        std::move(watcher_ptr));
  }
}

RenderThreadImpl::~RenderThreadImpl() {
  g_main_task_runner.Get() = nullptr;
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

  if (pumping_events) {
    renderer_paused_handle = main_thread_scheduler_->PauseRenderer();
    WebView::WillEnterModalLoop();
  }

  bool rv = ChildThreadImpl::Send(msg);

  if (pumping_events)
    WebView::DidExitModalLoop();

  return rv;
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
  auto it = pending_frame_creates_.find(routing_id);
  if (it == pending_frame_creates_.end())
    return;

  RenderFrameImpl* frame = RenderFrameImpl::FromRoutingID(routing_id);
  if (!frame)
    return;

  scoped_refptr<PendingFrameCreate> create(it->second);
  frame->BindFrame(it->second->browser_info(), it->second->TakeFrameRequest());
  pending_frame_creates_.erase(it);
}

void RenderThreadImpl::RemoveRoute(int32_t routing_id) {
  ChildThreadImpl::GetRouter()->RemoveRoute(routing_id);
}

void RenderThreadImpl::RegisterPendingFrameCreate(
    const service_manager::BindSourceInfo& browser_info,
    int routing_id,
    mojom::FrameRequest frame_request) {
  std::pair<PendingFrameCreateMap::iterator, bool> result =
      pending_frame_creates_.insert(std::make_pair(
          routing_id, base::MakeRefCounted<PendingFrameCreate>(
                          browser_info, routing_id, std::move(frame_request))));
  CHECK(result.second) << "Inserting a duplicate item.";
}

blink::mojom::StoragePartitionService*
RenderThreadImpl::GetStoragePartitionService() {
  return storage_partition_service_.get();
}

mojom::RendererHost* RenderThreadImpl::GetRendererHost() {
  if (!renderer_host_) {
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
        base::CreateSingleThreadTaskRunnerWithTraits(
            {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
  }

  return video_frame_compositor_task_runner_;
}

void RenderThreadImpl::InitializeWebKit(
    service_manager::BinderRegistry* registry) {
  DCHECK(!blink_platform_impl_);

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

#ifdef ENABLE_VTUNE_JIT_INTERFACE
  if (command_line.HasSwitch(switches::kEnableVtune))
    gin::Debug::SetJitCodeEventHandler(vTune::GetVtuneCodeEventHandler());
#endif

  blink_platform_impl_.reset(
      new RendererBlinkPlatformImpl(main_thread_scheduler_.get()));
  SetRuntimeFeaturesDefaultsAndUpdateFromArgs(command_line);
  GetContentClient()
      ->renderer()
      ->SetRuntimeFeaturesDefaultsBeforeBlinkInitialization();
  blink::Initialize(blink_platform_impl_.get(), registry,
                    main_thread_scheduler_.get());

  v8::Isolate* isolate = blink::MainThreadIsolate();
  isolate->SetCreateHistogramFunction(CreateHistogram);
  isolate->SetAddHistogramSampleFunction(AddHistogramSample);
  main_thread_scheduler_->AddRAILModeObserver(this);

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

  if (command_line.HasSwitch(switches::kExplicitlyAllowedPorts)) {
    std::string allowed_ports =
        command_line.GetSwitchValueASCII(switches::kExplicitlyAllowedPorts);
    net::SetExplicitlyAllowedPorts(allowed_ports);
  }
}

void RenderThreadImpl::RegisterSchemes() {
  // chrome:
  WebString chrome_scheme(WebString::FromASCII(kChromeUIScheme));
  WebSecurityPolicy::RegisterURLSchemeAsDisplayIsolated(chrome_scheme);
  WebSecurityPolicy::RegisterURLSchemeAsNotAllowingJavascriptURLs(
      chrome_scheme);

  // chrome-devtools:
  WebString devtools_scheme(WebString::FromASCII(kChromeDevToolsScheme));
  WebSecurityPolicy::RegisterURLSchemeAsDisplayIsolated(devtools_scheme);

  // view-source:
  WebString view_source_scheme(WebString::FromASCII(kViewSourceScheme));
  WebSecurityPolicy::RegisterURLSchemeAsDisplayIsolated(view_source_scheme);

  // chrome-error:
  WebString error_scheme(WebString::FromASCII(kChromeErrorScheme));
  WebSecurityPolicy::RegisterURLSchemeAsDisplayIsolated(error_scheme);
  WebSecurityPolicy::RegisterURLSchemeAsNotAllowingJavascriptURLs(error_scheme);
}

void RenderThreadImpl::RecordAction(const base::UserMetricsAction& action) {
  Send(new ViewHostMsg_UserMetricsRecordAction(action.str_));
}

void RenderThreadImpl::RecordComputedAction(const std::string& action) {
  Send(new ViewHostMsg_UserMetricsRecordAction(action));
}

std::unique_ptr<base::SharedMemory>
RenderThreadImpl::HostAllocateSharedMemoryBuffer(size_t size) {
  return ChildThreadImpl::AllocateSharedMemory(size);
}

void RenderThreadImpl::RegisterExtension(v8::Extension* extension) {
  WebScriptController::RegisterExtension(extension);
}

int RenderThreadImpl::PostTaskToAllWebWorkers(const base::Closure& closure) {
  return WorkerThreadRegistry::Instance()->PostTaskToAllThreads(closure);
}

bool RenderThreadImpl::ResolveProxy(const GURL& url, std::string* proxy_list) {
  bool result = false;
  Send(new ViewHostMsg_ResolveProxy(url, &result, proxy_list));
  return result;
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
  // This context is only used to create textures and mailbox them, so
  // use lower limits than the default.
  gpu::SharedMemoryLimits limits = gpu::SharedMemoryLimits::ForMailboxContext();
  bool support_locking = false;
  bool support_gles2_interface = true;
  bool support_raster_interface = false;
  bool support_oop_rasterization = false;
  bool support_grcontext = false;
  bool automatic_flushes = false;
  scoped_refptr<ws::ContextProviderCommandBuffer> media_context_provider =
      CreateOffscreenContext(
          gpu_channel_host, GetGpuMemoryBufferManager(), limits,
          support_locking, support_gles2_interface, support_raster_interface,
          support_oop_rasterization, support_grcontext, automatic_flushes,
          ws::command_buffer_metrics::ContextType::MEDIA, kGpuStreamIdMedia,
          kGpuStreamPriorityMedia);

  const bool enable_video_accelerator =
      !cmd_line->HasSwitch(switches::kDisableAcceleratedVideoDecode) &&
      (gpu_channel_host->gpu_feature_info()
           .status_values[gpu::GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE] ==
       gpu::kGpuFeatureStatusEnabled);
  const bool enable_gpu_memory_buffers =
      !is_gpu_compositing_disabled_ &&
#if defined(OS_MACOSX) || defined(OS_LINUX) || defined(OS_WIN)
      !cmd_line->HasSwitch(switches::kDisableGpuMemoryBufferVideoFrames);
#else
      cmd_line->HasSwitch(switches::kEnableGpuMemoryBufferVideoFrames);
#endif  // defined(OS_MACOSX) || defined(OS_LINUX) || defined(OS_WIN)
  const bool enable_media_stream_gpu_memory_buffers =
      enable_gpu_memory_buffers &&
      base::FeatureList::IsEnabled(
          features::kWebRtcUseGpuMemoryBufferVideoFrames);
  bool enable_video_gpu_memory_buffers = enable_gpu_memory_buffers;
#if defined(OS_WIN)
  enable_video_gpu_memory_buffers =
      enable_video_gpu_memory_buffers &&
      (cmd_line->HasSwitch(switches::kEnableGpuMemoryBufferVideoFrames) ||
       gpu_channel_host->gpu_info().direct_composition_overlays);
#endif  // defined(OS_WIN)

  media::mojom::InterfaceFactoryPtr interface_factory;
  GetConnector()->BindInterface(mojom::kBrowserServiceName, &interface_factory);

  media::mojom::VideoEncodeAcceleratorProviderPtr vea_provider;
  gpu_->CreateVideoEncodeAcceleratorProvider(mojo::MakeRequest(&vea_provider));

  gpu_factories_.push_back(GpuVideoAcceleratorFactoriesImpl::Create(
      std::move(gpu_channel_host), base::ThreadTaskRunnerHandle::Get(),
      GetMediaThreadTaskRunner(), std::move(media_context_provider),
      enable_video_gpu_memory_buffers, enable_media_stream_gpu_memory_buffers,
      enable_video_accelerator, interface_factory.PassInterface(),
      vea_provider.PassInterface()));
  gpu_factories_.back()->SetRenderingColorSpace(rendering_color_space_);
  return gpu_factories_.back().get();
}

scoped_refptr<viz::ContextProvider>
RenderThreadImpl::GetVideoFrameCompositorContextProvider(
    scoped_refptr<viz::ContextProvider> unwanted_context_provider) {
  if (video_frame_compositor_context_provider_ &&
      video_frame_compositor_context_provider_ != unwanted_context_provider) {
    return video_frame_compositor_context_provider_;
  }

  video_frame_compositor_context_provider_ = nullptr;

  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host =
      EstablishGpuChannelSync();
  if (!gpu_channel_host)
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
  video_frame_compositor_context_provider_ = CreateOffscreenContext(
      gpu_channel_host, GetGpuMemoryBufferManager(), limits, support_locking,
      support_gles2_interface, support_raster_interface,
      support_oop_rasterization, support_grcontext, automatic_flushes,
      ws::command_buffer_metrics::ContextType::RENDER_COMPOSITOR,
      kGpuStreamIdMedia, kGpuStreamPriorityMedia);
  return video_frame_compositor_context_provider_;
}

scoped_refptr<ws::ContextProviderCommandBuffer>
RenderThreadImpl::SharedMainThreadContextProvider() {
  DCHECK(IsMainThread());
  if (shared_main_thread_contexts_ &&
      shared_main_thread_contexts_->ContextGL()->GetGraphicsResetStatusKHR() ==
          GL_NO_ERROR)
    return shared_main_thread_contexts_;

  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host(
      EstablishGpuChannelSync());
  if (!gpu_channel_host) {
    shared_main_thread_contexts_ = nullptr;
    return nullptr;
  }

  bool support_locking = false;
  bool support_gles2_interface = true;
  bool support_raster_interface = false;
  bool support_oop_rasterization = false;
  bool support_grcontext = true;
  // Enable automatic flushes to improve canvas throughput.
  // See https://crbug.com/880901
  bool automatic_flushes = true;
  shared_main_thread_contexts_ = CreateOffscreenContext(
      std::move(gpu_channel_host), GetGpuMemoryBufferManager(),
      gpu::SharedMemoryLimits(), support_locking, support_gles2_interface,
      support_raster_interface, support_oop_rasterization, support_grcontext,
      automatic_flushes,
      ws::command_buffer_metrics::ContextType::RENDERER_MAIN_THREAD,
      kGpuStreamIdDefault, kGpuStreamPriorityDefault);
  auto result = shared_main_thread_contexts_->BindToCurrentThread();
  if (result != gpu::ContextResult::kSuccess)
    shared_main_thread_contexts_ = nullptr;
  return shared_main_thread_contexts_;
}

#if defined(OS_ANDROID)

scoped_refptr<StreamTextureFactory> RenderThreadImpl::GetStreamTexureFactory() {
  DCHECK(IsMainThread());
  if (!stream_texture_factory_.get() ||
      stream_texture_factory_->ContextGL()->GetGraphicsResetStatusKHR() !=
          GL_NO_ERROR) {
    scoped_refptr<ws::ContextProviderCommandBuffer> shared_context_provider =
        SharedMainThreadContextProvider();
    if (!shared_context_provider) {
      stream_texture_factory_ = nullptr;
      return nullptr;
    }
    DCHECK(shared_context_provider->GetCommandBufferProxy());
    DCHECK(shared_context_provider->GetCommandBufferProxy()->channel());
    stream_texture_factory_ =
        StreamTextureFactory::Create(std::move(shared_context_provider));
  }
  return stream_texture_factory_;
}

bool RenderThreadImpl::EnableStreamTextureCopy() {
  return GetContentClient()->UsingSynchronousCompositing();
}

#endif

AudioRendererMixerManager* RenderThreadImpl::GetAudioRendererMixerManager() {
  if (!audio_renderer_mixer_manager_) {
    audio_renderer_mixer_manager_ = AudioRendererMixerManager::Create();
  }

  return audio_renderer_mixer_manager_.get();
}

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
    blink::scheduler::RendererProcessType type) {
  main_thread_scheduler_->SetRendererProcessType(type);
}

blink::WebString RenderThreadImpl::GetUserAgent() const {
  DCHECK(!user_agent_.IsNull());
  return user_agent_;
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

bool RenderThreadImpl::IsGpuRasterizationForced() {
  return is_gpu_rasterization_forced_;
}

int RenderThreadImpl::GetGpuRasterizationMSAASampleCount() {
  return gpu_rasterization_msaa_sample_count_;
}

bool RenderThreadImpl::IsLcdTextEnabled() {
  return is_lcd_text_enabled_;
}

bool RenderThreadImpl::IsZeroCopyEnabled() {
  return is_zero_copy_enabled_;
}

bool RenderThreadImpl::IsPartialRasterEnabled() {
  return is_partial_raster_enabled_;
}

bool RenderThreadImpl::IsGpuMemoryBufferCompositorResourcesEnabled() {
  return is_gpu_memory_buffer_compositor_resources_enabled_;
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

scoped_refptr<base::SingleThreadTaskRunner>
RenderThreadImpl::GetCompositorImplThreadTaskRunner() {
  return compositor_task_runner_;
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
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableCompositorUkmForTests)) {
    return nullptr;
  }

  return std::make_unique<UkmRecorderFactoryImpl>(GetConnector()->Clone());
}

#ifdef OS_ANDROID
bool RenderThreadImpl::UsingSynchronousCompositing() {
  return GetContentClient()->UsingSynchronousCompositing();
}
#endif

void RenderThreadImpl::OnRAILModeChanged(v8::RAILMode rail_mode) {
  blink::MainThreadIsolate()->SetRAILMode(rail_mode);
  blink::SetRAILModeOnWorkerThreadIsolates(rail_mode);
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

  // Some messages are handled by delegates.
  if (dom_storage_dispatcher_->OnMessageReceived(msg)) {
    return true;
  }
  return false;
}

void RenderThreadImpl::SetSchedulerKeepActive(bool keep_active) {
  main_thread_scheduler_->SetSchedulerKeepActive(keep_active);
}

void RenderThreadImpl::SetProcessBackgrounded(bool backgrounded) {
  main_thread_scheduler_->SetRendererBackgrounded(backgrounded);
  if (backgrounded) {
    needs_to_record_first_active_paint_ = false;
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
  } else {
    process_foregrounded_count_++;
  }
}

void RenderThreadImpl::ProcessPurgeAndSuspend() {
  if (!RendererIsHidden())
    return;

  if (!base::FeatureList::IsEnabled(features::kPurgeAndSuspend))
    return;

  if (base::MemoryPressureListener::AreNotificationsSuppressed())
    return;

  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  needs_to_record_first_active_paint_ = true;

  RendererMemoryMetrics memory_metrics;
  if (!GetRendererMemoryMetrics(&memory_metrics))
    return;

  purge_and_suspend_memory_metrics_ = memory_metrics;
  GetWebMainThreadScheduler()->DefaultTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &RenderThreadImpl::RecordPurgeAndSuspendMemoryGrowthMetrics,
          base::Unretained(this), "30min", process_foregrounded_count_),
      base::TimeDelta::FromMinutes(30));
  GetWebMainThreadScheduler()->DefaultTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &RenderThreadImpl::RecordPurgeAndSuspendMemoryGrowthMetrics,
          base::Unretained(this), "60min", process_foregrounded_count_),
      base::TimeDelta::FromMinutes(60));
  GetWebMainThreadScheduler()->DefaultTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &RenderThreadImpl::RecordPurgeAndSuspendMemoryGrowthMetrics,
          base::Unretained(this), "90min", process_foregrounded_count_),
      base::TimeDelta::FromMinutes(90));
}

void RenderThreadImpl::SetIsLockedToSite() {
  DCHECK(blink_platform_impl_);
  blink_platform_impl_->SetIsLockedToSite();
}

void RenderThreadImpl::EnableV8LowMemoryMode() {
  if (!low_memory_mode_controller_)
    low_memory_mode_controller_.reset(new LowMemoryModeController());
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

  discardable_memory::ClientDiscardableSharedMemoryManager::Statistics
      discardable_stats = discardable_shared_memory_manager_->GetStatistics();
  size_t discardable_usage =
      discardable_stats.total_size - discardable_stats.freelist_size;
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

static void RecordPurgeAndSuspendMemoryGrowthKB(const char* basename,
                                                const char* suffix,
                                                int memory_usage) {
  std::string histogram_name = base::StringPrintf("%s.%s", basename, suffix);
  base::UmaHistogramMemoryKB(histogram_name, memory_usage);
}

void RenderThreadImpl::RecordPurgeAndSuspendMemoryGrowthMetrics(
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

  RecordPurgeAndSuspendMemoryGrowthKB(
      "PurgeAndSuspend.Experimental.MemoryGrowth.PartitionAllocKB", suffix,
      GET_MEMORY_GROWTH(memory_metrics, purge_and_suspend_memory_metrics_,
                        partition_alloc_kb));
  RecordPurgeAndSuspendMemoryGrowthKB(
      "PurgeAndSuspend.Experimental.MemoryGrowth.BlinkGCKB", suffix,
      GET_MEMORY_GROWTH(memory_metrics, purge_and_suspend_memory_metrics_,
                        blink_gc_kb));
  RecordPurgeAndSuspendMemoryGrowthKB(
      "PurgeAndSuspend.Experimental.MemoryGrowth.MallocKB", suffix,
      GET_MEMORY_GROWTH(memory_metrics, purge_and_suspend_memory_metrics_,
                        malloc_mb) *
          1024);
  RecordPurgeAndSuspendMemoryGrowthKB(
      "PurgeAndSuspend.Experimental.MemoryGrowth.DiscardableKB", suffix,
      GET_MEMORY_GROWTH(memory_metrics, purge_and_suspend_memory_metrics_,
                        discardable_kb));
  RecordPurgeAndSuspendMemoryGrowthKB(
      "PurgeAndSuspend.Experimental.MemoryGrowth.V8MainThreadIsolateKB", suffix,
      GET_MEMORY_GROWTH(memory_metrics, purge_and_suspend_memory_metrics_,
                        v8_main_thread_isolate_mb) *
          1024);
  RecordPurgeAndSuspendMemoryGrowthKB(
      "PurgeAndSuspend.Experimental.MemoryGrowth.TotalAllocatedKB", suffix,
      GET_MEMORY_GROWTH(memory_metrics, purge_and_suspend_memory_metrics_,
                        total_allocated_mb) *
          1024);
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
    int routing_id,
    scoped_refptr<FrameSwapMessageQueue> frame_swap_message_queue,
    const GURL& url,
    LayerTreeFrameSinkCallback callback,
    mojom::RenderFrameMetadataObserverClientRequest
        render_frame_metadata_observer_client_request,
    mojom::RenderFrameMetadataObserverPtr render_frame_metadata_observer_ptr,
    const char* client_name) {
  // Misconfigured bots (eg. crbug.com/780757) could run layout tests on a
  // machine where gpu compositing doesn't work. Don't crash in that case.
  if (layout_test_mode() && is_gpu_compositing_disabled_) {
    LOG(FATAL) << "Layout tests require gpu compositing, but it is disabled.";
    return;
  }

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  cc::mojo_embedder::AsyncLayerTreeFrameSink::InitParams params;
  params.compositor_task_runner = compositor_task_runner_;
  params.enable_surface_synchronization =
      features::IsSurfaceSynchronizationEnabled();
  params.local_surface_id_provider =
      std::make_unique<RendererLocalSurfaceIdProvider>();
  if (features::IsVizHitTestingDrawQuadEnabled()) {
    params.hit_test_data_provider =
        std::make_unique<viz::HitTestDataProviderDrawQuad>(
            true /* should_ask_for_child_region */,
            true /* root_accepts_events */);
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

#if defined(USE_AURA)
  if (features::IsMultiProcessMash()) {
    if (!RendererWindowTreeClient::Get(routing_id)) {
      std::move(callback).Run(nullptr);
      return;
    }
    scoped_refptr<gpu::GpuChannelHost> channel = EstablishGpuChannelSync();
    // If the channel could not be established correctly, then return null. This
    // would cause the compositor to wait and try again at a later time.
    if (!channel) {
      std::move(callback).Run(nullptr);
      return;
    }
    RendererWindowTreeClient::Get(routing_id)
        ->RequestLayerTreeFrameSink(
            gpu_->CreateContextProvider(std::move(channel)),
            GetGpuMemoryBufferManager(), std::move(callback));
    frame_sink_provider_->RegisterRenderFrameMetadataObserver(
        routing_id, std::move(render_frame_metadata_observer_client_request),
        std::move(render_frame_metadata_observer_ptr));
    return;
  }
#endif

  viz::mojom::CompositorFrameSinkRequest compositor_frame_sink_request =
      mojo::MakeRequest(&params.pipes.compositor_frame_sink_info);
  viz::mojom::CompositorFrameSinkClientPtr compositor_frame_sink_client;
  params.pipes.client_request =
      mojo::MakeRequest(&compositor_frame_sink_client);

  if (is_gpu_compositing_disabled_) {
    DCHECK(!layout_test_mode());
    frame_sink_provider_->CreateForWidget(
        routing_id, std::move(compositor_frame_sink_request),
        std::move(compositor_frame_sink_client));
    frame_sink_provider_->RegisterRenderFrameMetadataObserver(
        routing_id, std::move(render_frame_metadata_observer_client_request),
        std::move(render_frame_metadata_observer_ptr));
    std::move(callback).Run(
        std::make_unique<cc::mojo_embedder::AsyncLayerTreeFrameSink>(
            nullptr, nullptr, &params));
    return;
  }

  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host =
      EstablishGpuChannelSync();
  if (!gpu_channel_host) {
    // Wait and try again. We may hear that the compositing mode has switched
    // to software in the meantime.
    std::move(callback).Run(nullptr);
    return;
  }

  scoped_refptr<viz::RasterContextProvider> worker_context_provider =
      SharedCompositorWorkerContextProvider(/*try_gpu_rasterization=*/true);
  if (!worker_context_provider) {
    // Cause the compositor to wait and try again.
    std::move(callback).Run(nullptr);
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

  scoped_refptr<ws::ContextProviderCommandBuffer> context_provider(
      new ws::ContextProviderCommandBuffer(
          gpu_channel_host, GetGpuMemoryBufferManager(), kGpuStreamIdDefault,
          kGpuStreamPriorityDefault, gpu::kNullSurfaceHandle, url,
          automatic_flushes, support_locking, support_grcontext, limits,
          attributes,
          ws::command_buffer_metrics::ContextType::RENDER_COMPOSITOR));

  if (layout_test_deps_) {
    if (!layout_test_deps_->UseDisplayCompositorPixelDump()) {
      std::move(callback).Run(layout_test_deps_->CreateLayerTreeFrameSink(
          routing_id, std::move(gpu_channel_host), std::move(context_provider),
          std::move(worker_context_provider), GetGpuMemoryBufferManager(),
          this));
      return;
    } else if (!params.compositor_task_runner) {
      // The frame sink provider expects a compositor task runner, but we might
      // not have that if we're running layout tests in single threaded mode.
      // Set it to be our thread's task runner instead.
      params.compositor_task_runner = GetCompositorMainThreadTaskRunner();
    }
  }

#if defined(OS_ANDROID)
  if (GetContentClient()->UsingSynchronousCompositing()) {
    RenderWidget* widget = RenderWidget::FromRoutingID(routing_id);
    if (widget) {
      std::move(callback).Run(std::make_unique<SynchronousLayerTreeFrameSink>(
          std::move(context_provider), std::move(worker_context_provider),
          compositor_task_runner_, GetGpuMemoryBufferManager(),
          sync_message_filter(), routing_id, g_next_layer_tree_frame_sink_id++,
          std::move(params.synthetic_begin_frame_source),
          widget->widget_input_handler_manager()
              ->GetSynchronousCompositorRegistry(),
          std::move(frame_swap_message_queue)));
      return;
    } else {
      NOTREACHED();
    }
  }
#endif
  frame_sink_provider_->CreateForWidget(
      routing_id, std::move(compositor_frame_sink_request),
      std::move(compositor_frame_sink_client));
  frame_sink_provider_->RegisterRenderFrameMetadataObserver(
      routing_id, std::move(render_frame_metadata_observer_client_request),
      std::move(render_frame_metadata_observer_ptr));
  params.gpu_memory_buffer_manager = GetGpuMemoryBufferManager();
  std::move(callback).Run(
      std::make_unique<cc::mojo_embedder::AsyncLayerTreeFrameSink>(
          std::move(context_provider), std::move(worker_context_provider),
          &params));
}

blink::AssociatedInterfaceRegistry*
RenderThreadImpl::GetAssociatedInterfaceRegistry() {
  return &associated_interfaces_;
}

std::unique_ptr<cc::SwapPromise>
RenderThreadImpl::RequestCopyOfOutputForLayoutTest(
    int32_t routing_id,
    std::unique_ptr<viz::CopyOutputRequest> request) {
  DCHECK(layout_test_deps_ &&
         !layout_test_deps_->UseDisplayCompositorPixelDump());
  return layout_test_deps_->RequestCopyOfOutput(routing_id, std::move(request));
}

std::unique_ptr<blink::WebMediaStreamCenter>
RenderThreadImpl::CreateMediaStreamCenter() {
  std::unique_ptr<blink::WebMediaStreamCenter> media_stream_center;
  if (!media_stream_center) {
    media_stream_center = std::make_unique<MediaStreamCenter>();
  }
  return media_stream_center;
}

PeerConnectionDependencyFactory*
RenderThreadImpl::GetPeerConnectionDependencyFactory() {
  return peer_connection_factory_.get();
}

mojom::RenderFrameMessageFilter*
RenderThreadImpl::render_frame_message_filter() {
  if (!render_frame_message_filter_)
    GetChannel()->GetRemoteAssociatedInterface(&render_frame_message_filter_);
  return render_frame_message_filter_.get();
}

mojom::RenderMessageFilter* RenderThreadImpl::render_message_filter() {
  if (!render_message_filter_)
    GetChannel()->GetRemoteAssociatedInterface(&render_message_filter_);
  return render_message_filter_.get();
}

gpu::GpuChannelHost* RenderThreadImpl::GetGpuChannel() {
  return gpu_->GetGpuChannel().get();
}

void RenderThreadImpl::CreateEmbedderRendererService(
    service_manager::mojom::ServiceRequest service_request) {
  GetContentClient()->renderer()->CreateRendererService(
      std::move(service_request));
}

void RenderThreadImpl::CreateView(mojom::CreateViewParamsPtr params) {
  CompositorDependencies* compositor_deps = this;
  is_scroll_animator_enabled_ = params->web_preferences.enable_scroll_animator;
  // When bringing in render_view, also bring in webkit's glue and jsbindings.
  RenderViewImpl::Create(compositor_deps, std::move(params),
                         RenderWidget::ShowCallback(),
                         GetWebMainThreadScheduler()->DefaultTaskRunner());
}

void RenderThreadImpl::CreateFrame(mojom::CreateFrameParamsPtr params) {
  CompositorDependencies* compositor_deps = this;
  service_manager::mojom::InterfaceProviderPtr interface_provider(
      std::move(params->interface_provider));
  RenderFrameImpl::CreateFrame(
      params->routing_id, std::move(interface_provider),
      params->proxy_routing_id, params->opener_routing_id,
      params->parent_routing_id, params->previous_sibling_routing_id,
      params->devtools_frame_token, params->replication_state, compositor_deps,
      *params->widget_params, params->frame_owner_properties,
      params->has_committed_real_load);
}

void RenderThreadImpl::CreateFrameProxy(
    int32_t routing_id,
    int32_t render_view_routing_id,
    int32_t opener_routing_id,
    int32_t parent_routing_id,
    const FrameReplicationState& replicated_state,
    const base::UnguessableToken& devtools_frame_token) {
  RenderFrameProxy::CreateFrameProxy(
      routing_id, render_view_routing_id,
      RenderFrameImpl::ResolveOpener(opener_routing_id), parent_routing_id,
      replicated_state, devtools_frame_token);
}

void RenderThreadImpl::SetUpEmbeddedWorkerChannelForServiceWorker(
    mojom::EmbeddedWorkerInstanceClientRequest client_request) {
  EmbeddedWorkerInstanceClientImpl::Create(GetIOTaskRunner(),
                                           std::move(client_request));
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
  UMA_HISTOGRAM_BOOLEAN("NQE.RenderThreadNotified", true);
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
}

void RenderThreadImpl::UpdateScrollbarTheme(
    mojom::UpdateScrollbarThemeParamsPtr params) {
#if defined(OS_MACOSX)
  blink::WebScrollbarTheme::UpdateScrollbarsWithNSDefaults(
      params->initial_button_delay, params->autoscroll_button_delay,
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
#if defined(OS_MACOSX)
  SystemColorsDidChange(aqua_color_variant, highlight_text_color,
                        highlight_color);
#else
  NOTREACHED();
#endif
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
  TRACE_EVENT0("memory", "RenderThreadImpl::OnMemoryPressure");
  if (blink_platform_impl_) {
    blink::WebMemoryCoordinator::OnMemoryPressure(
        static_cast<blink::WebMemoryPressureLevel>(memory_pressure_level));
  }
  if (memory_pressure_level ==
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL)
    ReleaseFreeMemory();
}

void RenderThreadImpl::RecordPurgeMemory(RendererMemoryMetrics before) {
  RendererMemoryMetrics after;
  if (!GetRendererMemoryMetrics(&after))
    return;
  int64_t mbytes = static_cast<int64_t>(before.total_allocated_mb) -
                   static_cast<int64_t>(after.total_allocated_mb);
  if (mbytes < 0)
    mbytes = 0;
  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Experimental.Renderer.PurgedMemory",
                                mbytes);
}

scoped_refptr<base::SingleThreadTaskRunner>
RenderThreadImpl::GetMediaThreadTaskRunner() {
  DCHECK(main_thread_runner()->BelongsToCurrentThread());
  if (!media_thread_) {
    media_thread_.reset(new base::Thread("Media"));
    media_thread_->Start();
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
      (IsGpuRasterizationForced() ||
       gpu_channel_host->gpu_feature_info()
               .status_values[gpu::GPU_FEATURE_TYPE_GPU_RASTERIZATION] ==
           gpu::kGpuFeatureStatusEnabled);
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
      automatic_flushes, ws::command_buffer_metrics::ContextType::RENDER_WORKER,
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
      bool forced_or_supported =
          IsGpuRasterizationForced() ||
          shared_worker_context_provider_->ContextCapabilities()
              .gpu_rasterization;
      if (forced_or_supported &&
          shared_worker_context_provider_->ContextSupport()
              ->HasGrContextSupport()) {
        // Do not check GrContext above. It is lazy-created, and we only want to
        // create it if it might be used.
        GrContext* gr_context = shared_worker_context_provider_->GrContext();
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
  return widget_count_ > 0 && hidden_widget_count_ == widget_count_;
}

void RenderThreadImpl::WidgetCreated() {
  bool renderer_was_hidden = RendererIsHidden();
  widget_count_++;
  if (renderer_was_hidden)
    OnRendererVisible();
}

void RenderThreadImpl::WidgetDestroyed() {
  // TODO(rmcilroy): Remove the restriction that destroyed widgets must be
  // unhidden before WidgetDestroyed is called.
  DCHECK_GT(widget_count_, 0);
  DCHECK_GT(widget_count_, hidden_widget_count_);
  widget_count_--;
  if (RendererIsHidden())
    OnRendererHidden();
}

void RenderThreadImpl::WidgetHidden() {
  DCHECK_LT(hidden_widget_count_, widget_count_);
  hidden_widget_count_++;
  if (RendererIsHidden())
    OnRendererHidden();
}

void RenderThreadImpl::WidgetRestored() {
  bool renderer_was_hidden = RendererIsHidden();
  DCHECK_GT(hidden_widget_count_, 0);
  hidden_widget_count_--;
  if (renderer_was_hidden)
    OnRendererVisible();
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

void RenderThreadImpl::ReleaseFreeMemory() {
  base::allocator::ReleaseFreeMemory();
  discardable_shared_memory_manager_->ReleaseFreeMemory();

  // Do not call into blink if it is not initialized.
  if (blink_platform_impl_) {
    // Purge Skia font cache, resource cache, and image filter.
    SkGraphics::PurgeAllCaches();
    blink::WebMemoryCoordinator::OnPurgeMemory();
  }
}

RenderThreadImpl::PendingFrameCreate::PendingFrameCreate(
    const service_manager::BindSourceInfo& browser_info,
    int routing_id,
    mojom::FrameRequest frame_request)
    : browser_info_(browser_info),
      routing_id_(routing_id),
      frame_request_(std::move(frame_request)) {}

RenderThreadImpl::PendingFrameCreate::~PendingFrameCreate() {
}

void RenderThreadImpl::PendingFrameCreate::OnConnectionError() {
  size_t erased =
      RenderThreadImpl::current()->pending_frame_creates_.erase(routing_id_);
  DCHECK_EQ(1u, erased);
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

void RenderThreadImpl::OnRendererInterfaceRequest(
    mojom::RendererAssociatedRequest request) {
  DCHECK(!renderer_binding_.is_bound());
  renderer_binding_.Bind(std::move(request),
                         GetWebMainThreadScheduler()->IPCTaskRunner());
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

}  // namespace content
