// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_THREAD_IMPL_H_
#define CONTENT_RENDERER_RENDER_THREAD_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/user_metrics_action.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "content/child/child_thread_impl.h"
#include "content/common/content_export.h"
#include "content/common/frame.mojom.h"
#include "content/common/frame_replication_state.h"
#include "content/common/frame_sink_provider.mojom.h"
#include "content/common/render_frame_message_filter.mojom.h"
#include "content/common/render_frame_metadata.mojom.h"
#include "content/common/render_message_filter.mojom.h"
#include "content/common/renderer.mojom.h"
#include "content/common/renderer_host.mojom.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/url_loader_throttle_provider.h"
#include "content/renderer/gpu/compositor_dependencies.h"
#include "content/renderer/layout_test_dependencies.h"
#include "content/renderer/media/audio/audio_input_ipc_factory.h"
#include "content/renderer/media/audio/audio_output_ipc_factory.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "ipc/ipc_sync_channel.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/thread_safe_interface_ptr.h"
#include "net/base/network_change_notifier.h"
#include "net/nqe/effective_connection_type.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/viz/public/interfaces/compositing/compositing_mode_watcher.mojom.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/mojom/dom_storage/storage_partition_service.mojom.h"
#include "third_party/blink/public/platform/scheduler/web_rail_mode_observer.h"
#include "third_party/blink/public/platform/web_connection_type.h"
#include "third_party/blink/public/web/web_memory_statistics.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_MACOSX)
#include "third_party/blink/public/platform/mac/web_scrollbar_theme.h"
#endif

class SkBitmap;

namespace blink {
class WebMediaStreamCenter;
}

namespace base {
class SingleThreadTaskRunner;
class Thread;
}

namespace cc {
class BeginFrameSource;
class LayerTreeFrameSink;
class SyntheticBeginFrameSource;
class TaskGraphRunner;
}

namespace discardable_memory {
class ClientDiscardableSharedMemoryManager;
}

namespace gpu {
class GpuChannelHost;
}

namespace IPC {
class MessageFilter;
}

namespace media {
class GpuVideoAcceleratorFactories;
}

namespace v8 {
class Extension;
}

namespace viz {
class BeginFrameSource;
class RasterContextProvider;
class SyntheticBeginFrameSource;
}

namespace ws {
class ContextProviderCommandBuffer;
class Gpu;
}  // namespace ws

namespace content {

class AppCacheDispatcher;
class AecDumpMessageFilter;
class AudioRendererMixerManager;
class BrowserPluginManager;
class CategorizedWorkerPool;
class DomStorageDispatcher;
class FrameSwapMessageQueue;
class GpuVideoAcceleratorFactoriesImpl;
class LowMemoryModeController;
class MidiMessageFilter;
class P2PSocketDispatcher;
class PeerConnectionDependencyFactory;
class PeerConnectionTracker;
class RenderThreadObserver;
class RendererBlinkPlatformImpl;
class ResourceDispatcher;
class VideoCaptureImplManager;

#if defined(OS_ANDROID)
class StreamTextureFactory;
#endif

#if defined(COMPILER_MSVC)
// See explanation for other RenderViewHostImpl which is the same issue.
#pragma warning(push)
#pragma warning(disable: 4250)
#endif

// The RenderThreadImpl class represents a background thread where RenderView
// instances live.  The RenderThread supports an API that is used by its
// consumer to talk indirectly to the RenderViews and supporting objects.
// Likewise, it provides an API for the RenderViews to talk back to the main
// process (i.e., their corresponding WebContentsImpl).
//
// Most of the communication occurs in the form of IPC messages.  They are
// routed to the RenderThread according to the routing IDs of the messages.
// The routing IDs correspond to RenderView instances.
class CONTENT_EXPORT RenderThreadImpl
    : public RenderThread,
      public ChildThreadImpl,
      public blink::scheduler::WebRAILModeObserver,
      public mojom::Renderer,
      public viz::mojom::CompositingModeWatcher,
      public CompositorDependencies {
 public:
  static RenderThreadImpl* current();
  static mojom::RenderMessageFilter* current_render_message_filter();
  static RendererBlinkPlatformImpl* current_blink_platform_impl();

  static void SetRenderMessageFilterForTesting(
      mojom::RenderMessageFilter* render_message_filter);
  static void SetRendererBlinkPlatformImplForTesting(
      RendererBlinkPlatformImpl* blink_platform_impl);

  // Returns the task runner for the main thread where the RenderThread lives.
  static scoped_refptr<base::SingleThreadTaskRunner>
  DeprecatedGetMainTaskRunner();

  RenderThreadImpl(
      base::RepeatingClosure quit_closure,
      std::unique_ptr<blink::scheduler::WebThreadScheduler> scheduler);
  RenderThreadImpl(
      const InProcessChildThreadParams& params,
      std::unique_ptr<blink::scheduler::WebThreadScheduler> scheduler);
  ~RenderThreadImpl() override;
  void Shutdown() override;
  bool ShouldBeDestroyed() override;

  // When initializing WebKit, ensure that any schemes needed for the content
  // module are registered properly.  Static to allow sharing with tests.
  static void RegisterSchemes();

  // RenderThread implementation:
  bool Send(IPC::Message* msg) override;
  IPC::SyncChannel* GetChannel() override;
  std::string GetLocale() override;
  IPC::SyncMessageFilter* GetSyncMessageFilter() override;
  void AddRoute(int32_t routing_id, IPC::Listener* listener) override;
  void RemoveRoute(int32_t routing_id) override;
  int GenerateRoutingID() override;
  void AddFilter(IPC::MessageFilter* filter) override;
  void RemoveFilter(IPC::MessageFilter* filter) override;
  void AddObserver(RenderThreadObserver* observer) override;
  void RemoveObserver(RenderThreadObserver* observer) override;
  void SetResourceDispatcherDelegate(
      ResourceDispatcherDelegate* delegate) override;
  std::unique_ptr<base::SharedMemory> HostAllocateSharedMemoryBuffer(
      size_t buffer_size) override;
  void RegisterExtension(v8::Extension* extension) override;
  int PostTaskToAllWebWorkers(const base::Closure& closure) override;
  bool ResolveProxy(const GURL& url, std::string* proxy_list) override;
  base::WaitableEvent* GetShutdownEvent() override;
  int32_t GetClientId() override;
  bool IsOnline() override;
  void SetRendererProcessType(
      blink::scheduler::RendererProcessType type) override;
  blink::WebString GetUserAgent() const override;

  // IPC::Listener implementation via ChildThreadImpl:
  void OnAssociatedInterfaceRequest(
      const std::string& name,
      mojo::ScopedInterfaceEndpointHandle handle) override;

  // ChildThread implementation via ChildThreadImpl:
  scoped_refptr<base::SingleThreadTaskRunner> GetIOTaskRunner() override;

  // CompositorDependencies implementation.
  bool IsGpuRasterizationForced() override;
  int GetGpuRasterizationMSAASampleCount() override;
  bool IsLcdTextEnabled() override;
  bool IsZeroCopyEnabled() override;
  bool IsPartialRasterEnabled() override;
  bool IsGpuMemoryBufferCompositorResourcesEnabled() override;
  bool IsElasticOverscrollEnabled() override;
  bool IsUseZoomForDSFEnabled() override;
  scoped_refptr<base::SingleThreadTaskRunner>
  GetCompositorMainThreadTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner>
  GetCompositorImplThreadTaskRunner() override;
  blink::scheduler::WebThreadScheduler* GetWebMainThreadScheduler() override;
  cc::TaskGraphRunner* GetTaskGraphRunner() override;
  bool IsScrollAnimatorEnabled() override;
  std::unique_ptr<cc::UkmRecorderFactory> CreateUkmRecorderFactory() override;
#ifdef OS_ANDROID
  bool UsingSynchronousCompositing() override;
#endif

  bool IsThreadedAnimationEnabled();

  // blink::scheduler::WebRAILModeObserver implementation.
  void OnRAILModeChanged(v8::RAILMode rail_mode) override;

  // viz::mojom::CompositingModeWatcher implementation.
  void CompositingModeFallbackToSoftware() override;

  // Whether gpu compositing is being used or is disabled for software
  // compositing. Clients of the compositor should give resources that match
  // the appropriate mode.
  bool IsGpuCompositingDisabled() { return is_gpu_compositing_disabled_; }

  // Synchronously establish a channel to the GPU plugin if not previously
  // established or if it has been lost (for example if the GPU plugin crashed).
  // If there is a pending asynchronous request, it will be completed by the
  // time this routine returns.
  scoped_refptr<gpu::GpuChannelHost> EstablishGpuChannelSync();

  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager();

  using LayerTreeFrameSinkCallback =
      base::OnceCallback<void(std::unique_ptr<cc::LayerTreeFrameSink>)>;
  void RequestNewLayerTreeFrameSink(
      int routing_id,
      scoped_refptr<FrameSwapMessageQueue> frame_swap_message_queue,
      const GURL& url,
      LayerTreeFrameSinkCallback callback,
      mojom::RenderFrameMetadataObserverClientRequest
          render_frame_metadata_observer_client_request,
      mojom::RenderFrameMetadataObserverPtr render_frame_metadata_observer_ptr,
      const char* client_name);

  blink::AssociatedInterfaceRegistry* GetAssociatedInterfaceRegistry();

  std::unique_ptr<cc::SwapPromise> RequestCopyOfOutputForLayoutTest(
      int32_t routing_id,
      std::unique_ptr<viz::CopyOutputRequest> request);

  // True if we are running layout tests. This currently disables forwarding
  // various status messages to the console, skips network error pages, and
  // short circuits size update and focus events.
  bool layout_test_mode() const { return !!layout_test_deps_; }
  void set_layout_test_dependencies(
      std::unique_ptr<LayoutTestDependencies> deps) {
    layout_test_deps_ = std::move(deps);
  }
  // Returns whether we are running layout tests with display compositor for
  // pixel dump enabled. It is meant to disable feature that require display
  // compositor while it is not enabled by default.
  // This should only be called if currently running in layout tests.
  bool LayoutTestModeUsesDisplayCompositorPixelDump() const {
    DCHECK(layout_test_deps_);
    return layout_test_deps_->UseDisplayCompositorPixelDump();
  }

  discardable_memory::ClientDiscardableSharedMemoryManager*
  GetDiscardableSharedMemoryManagerForTest() {
    return discardable_shared_memory_manager_.get();
  }

  RendererBlinkPlatformImpl* blink_platform_impl() const {
    DCHECK(blink_platform_impl_);
    return blink_platform_impl_.get();
  }

  // Returns the task runner on the compositor thread.
  //
  // Will be null if threaded compositing has not been enabled.
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner() const {
    return compositor_task_runner_;
  }

  AppCacheDispatcher* appcache_dispatcher() const {
    return appcache_dispatcher_.get();
  }

  DomStorageDispatcher* dom_storage_dispatcher() const {
    return dom_storage_dispatcher_.get();
  }

  MidiMessageFilter* midi_message_filter() {
    return midi_message_filter_.get();
  }

  ResourceDispatcher* resource_dispatcher() const {
    return resource_dispatcher_.get();
  }

  URLLoaderThrottleProvider* url_loader_throttle_provider() const {
    return url_loader_throttle_provider_.get();
  }

#if defined(OS_ANDROID)
  scoped_refptr<StreamTextureFactory> GetStreamTexureFactory();
  bool EnableStreamTextureCopy();
#endif

  // Creates the embedder implementation of WebMediaStreamCenter.
  // The resulting object is owned by WebKit and deleted by WebKit at tear-down.
  std::unique_ptr<blink::WebMediaStreamCenter> CreateMediaStreamCenter();

  BrowserPluginManager* browser_plugin_manager() const {
    return browser_plugin_manager_.get();
  }

  // Returns a factory used for creating RTC PeerConnection objects.
  PeerConnectionDependencyFactory* GetPeerConnectionDependencyFactory();

  PeerConnectionTracker* peer_connection_tracker() {
    return peer_connection_tracker_.get();
  }

  // Current P2PSocketDispatcher. Set to NULL if P2P API is disabled.
  P2PSocketDispatcher* p2p_socket_dispatcher() {
    return p2p_socket_dispatcher_.get();
  }

  VideoCaptureImplManager* video_capture_impl_manager() const {
    return vc_manager_.get();
  }

  LowMemoryModeController* low_memory_mode_controller() const {
    return low_memory_mode_controller_.get();
  }

  mojom::RenderFrameMessageFilter* render_frame_message_filter();
  mojom::RenderMessageFilter* render_message_filter();

  // Get the GPU channel. Returns NULL if the channel is not established or
  // has been lost.
  gpu::GpuChannelHost* GetGpuChannel();

  // Returns a SingleThreadTaskRunner instance corresponding to the message loop
  // of the thread on which media operations should be run. Must be called
  // on the renderer's main thread.
  scoped_refptr<base::SingleThreadTaskRunner> GetMediaThreadTaskRunner();

  // A TaskRunner instance that runs tasks on the raster worker pool.
  base::TaskRunner* GetWorkerTaskRunner();

  // Creates a ContextProvider if yet created, and returns it to be used for
  // video frame compositing. The ContextProvider given as an argument is
  // one that has been lost, and is a hint to the RenderThreadImpl to clear
  // it's |video_frame_compositor_context_provider_| if it matches.
  scoped_refptr<viz::ContextProvider> GetVideoFrameCompositorContextProvider(
      scoped_refptr<viz::ContextProvider>);

  // Returns a worker context provider that will be bound on the compositor
  // thread.
  scoped_refptr<viz::RasterContextProvider>
  SharedCompositorWorkerContextProvider(bool try_gpu_rasterization);

  media::GpuVideoAcceleratorFactories* GetGpuFactories();

  scoped_refptr<ws::ContextProviderCommandBuffer>
  SharedMainThreadContextProvider();

  // AudioRendererMixerManager instance which manages renderer side mixer
  // instances shared based on configured audio parameters.  Lazily created on
  // first call.
  AudioRendererMixerManager* GetAudioRendererMixerManager();

#if defined(OS_WIN)
  void PreCacheFontCharacters(const LOGFONT& log_font,
                              const base::string16& str);
#endif

  // For producing custom V8 histograms. Custom histograms are produced if all
  // RenderViews share the same host, and the host is in the pre-specified set
  // of hosts we want to produce custom diagrams for. The name for a custom
  // diagram is the name of the corresponding generic diagram plus a
  // host-specific suffix.
  class CONTENT_EXPORT HistogramCustomizer {
   public:
    HistogramCustomizer();
    ~HistogramCustomizer();

    // Called when a top frame of a RenderView navigates. This function updates
    // RenderThreadImpl's information about whether all RenderViews are
    // displaying a page from the same host. |host| is the host where a
    // RenderView navigated, and |view_count| is the number of RenderViews in
    // this process.
    void RenderViewNavigatedToHost(const std::string& host, size_t view_count);

    // Used for customizing some histograms if all RenderViews share the same
    // host. Returns the current custom histogram name to use for
    // |histogram_name|, or |histogram_name| if it shouldn't be customized.
    std::string ConvertToCustomHistogramName(const char* histogram_name) const;

   private:
    FRIEND_TEST_ALL_PREFIXES(RenderThreadImplUnittest,
                             IdentifyAlexaTop10NonGoogleSite);
    friend class RenderThreadImplUnittest;

    // Converts a host name to a suffix for histograms
    std::string HostToCustomHistogramSuffix(const std::string& host);

    // Helper function to identify a certain set of top pages
    bool IsAlexaTop10NonGoogleSite(const std::string& host);

    // Used for updating the information on which is the common host which all
    // RenderView's share (if any). If there is no common host, this function is
    // called with an empty string.
    void SetCommonHost(const std::string& host);

    // The current common host of the RenderViews; empty string if there is no
    // common host.
    std::string common_host_;
    // The corresponding suffix.
    std::string common_host_histogram_suffix_;
    // Set of histograms for which we want to produce a custom histogram if
    // possible.
    std::set<std::string> custom_histograms_;

    DISALLOW_COPY_AND_ASSIGN(HistogramCustomizer);
  };

  HistogramCustomizer* histogram_customizer() {
    return &histogram_customizer_;
  }

  // Called by a RenderWidget when it is created or destroyed. This
  // allows the process to know when there are no visible widgets.
  void WidgetCreated();
  // Note: A widget must not be hidden when it is destroyed - ensure that
  // WidgetRestored is called before WidgetDestroyed for any hidden widget.
  void WidgetDestroyed();
  void WidgetHidden();
  void WidgetRestored();

  void RegisterPendingFrameCreate(
      const service_manager::BindSourceInfo& source_info,
      int routing_id,
      mojom::FrameRequest frame);

  blink::mojom::StoragePartitionService* GetStoragePartitionService();
  mojom::RendererHost* GetRendererHost();

  struct RendererMemoryMetrics {
    size_t partition_alloc_kb;
    size_t blink_gc_kb;
    size_t malloc_mb;
    size_t discardable_kb;
    size_t v8_main_thread_isolate_mb;
    size_t total_allocated_mb;
    size_t non_discardable_total_allocated_mb;
    size_t total_allocated_per_render_view_mb;
  };
  bool GetRendererMemoryMetrics(RendererMemoryMetrics* memory_metrics) const;

  bool NeedsToRecordFirstActivePaint(int metric_type) const;

  // Sets the current pipeline rendering color space.
  void SetRenderingColorSpace(const gfx::ColorSpace& color_space);

  scoped_refptr<base::SingleThreadTaskRunner>
  CreateVideoFrameCompositorTaskRunner();

 private:
  void OnProcessFinalRelease() override;
  // IPC::Listener
  void OnChannelError() override;

  // ChildThread
  bool OnControlMessageReceived(const IPC::Message& msg) override;
  void RecordAction(const base::UserMetricsAction& action) override;
  void RecordComputedAction(const std::string& action) override;

  bool IsMainThread();

  void RecordPurgeMemory(RendererMemoryMetrics before);

  void Init();
  void InitializeCompositorThread();
  void InitializeWebKit(service_manager::BinderRegistry* registry);

  void OnTransferBitmap(const SkBitmap& bitmap, int resource_id);
  void OnGetAccessibilityTree();

  // mojom::Renderer:
  void CreateEmbedderRendererService(
      service_manager::mojom::ServiceRequest service_request) override;
  void CreateView(mojom::CreateViewParamsPtr params) override;
  void CreateFrame(mojom::CreateFrameParamsPtr params) override;
  void CreateFrameProxy(
      int32_t routing_id,
      int32_t render_view_routing_id,
      int32_t opener_routing_id,
      int32_t parent_routing_id,
      const FrameReplicationState& replicated_state,
      const base::UnguessableToken& devtools_frame_token) override;
  void SetUpEmbeddedWorkerChannelForServiceWorker(
      mojom::EmbeddedWorkerInstanceClientRequest client_request) override;
  void OnNetworkConnectionChanged(
      net::NetworkChangeNotifier::ConnectionType type,
      double max_bandwidth_mbps) override;
  void OnNetworkQualityChanged(net::EffectiveConnectionType type,
                               base::TimeDelta http_rtt,
                               base::TimeDelta transport_rtt,
                               double bandwidth_kbps) override;
  void SetWebKitSharedTimersSuspended(bool suspend) override;
  void SetUserAgent(const std::string& user_agent) override;
  void UpdateScrollbarTheme(
      mojom::UpdateScrollbarThemeParamsPtr params) override;
  void OnSystemColorsChanged(int32_t aqua_color_variant,
                             const std::string& highlight_text_color,
                             const std::string& highlight_color) override;
  void PurgePluginListCache(bool reload_pages) override;
  void SetProcessBackgrounded(bool backgrounded) override;
  void SetSchedulerKeepActive(bool keep_active) override;
  void ProcessPurgeAndSuspend() override;
  void SetIsLockedToSite() override;
  void EnableV8LowMemoryMode() override;

  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

  bool RendererIsHidden() const;
  void OnRendererHidden();
  void OnRendererVisible();

  void RecordMemoryUsageAfterBackgrounded(const char* suffix,
                                          int foregrounded_count);
  void RecordPurgeAndSuspendMemoryGrowthMetrics(
      const char* suffix,
      int foregrounded_count_when_purged);

  void ReleaseFreeMemory();

  void OnSyncMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

  std::unique_ptr<viz::SyntheticBeginFrameSource>
  CreateSyntheticBeginFrameSource();

  void OnRendererInterfaceRequest(mojom::RendererAssociatedRequest request);

  std::unique_ptr<discardable_memory::ClientDiscardableSharedMemoryManager>
      discardable_shared_memory_manager_;

  // These objects live solely on the render thread.
  std::unique_ptr<AppCacheDispatcher> appcache_dispatcher_;
  std::unique_ptr<DomStorageDispatcher> dom_storage_dispatcher_;
  std::unique_ptr<blink::scheduler::WebThreadScheduler> main_thread_scheduler_;
  std::unique_ptr<RendererBlinkPlatformImpl> blink_platform_impl_;
  std::unique_ptr<ResourceDispatcher> resource_dispatcher_;
  std::unique_ptr<URLLoaderThrottleProvider> url_loader_throttle_provider_;

  // Used on the renderer and IPC threads.
  scoped_refptr<MidiMessageFilter> midi_message_filter_;

  std::unique_ptr<BrowserPluginManager> browser_plugin_manager_;

  std::unique_ptr<PeerConnectionDependencyFactory> peer_connection_factory_;

  // This is used to communicate to the browser process the status
  // of all the peer connections created in the renderer.
  std::unique_ptr<PeerConnectionTracker> peer_connection_tracker_;

  // Dispatches all P2P sockets.
  scoped_refptr<P2PSocketDispatcher> p2p_socket_dispatcher_;

  // Used for communicating registering AEC dump consumers with the browser and
  // receving AEC dump file handles when AEC dump is enabled. An AEC dump is
  // diagnostic audio data for WebRTC stored locally when enabled by the user in
  // chrome://webrtc-internals.
  scoped_refptr<AecDumpMessageFilter> aec_dump_message_filter_;

  // Provides AudioInputIPC objects for audio input devices. Initialized in
  // Init.
  base::Optional<AudioInputIPCFactory> audio_input_ipc_factory_;
  // Provides AudioOutputIPC objects for audio output devices. Initialized in
  // Init.
  base::Optional<AudioOutputIPCFactory> audio_output_ipc_factory_;

  // Used on the render thread.
  std::unique_ptr<VideoCaptureImplManager> vc_manager_;

  // The count of RenderWidgets running through this thread.
  int widget_count_;

  // The count of hidden RenderWidgets running through this thread.
  int hidden_widget_count_;

  blink::WebString user_agent_;

  // Used to control layout test specific behavior.
  std::unique_ptr<LayoutTestDependencies> layout_test_deps_;

  // Sticky once true, indicates that compositing is done without Gpu, so
  // resources given to the compositor or to the viz service should be
  // software-based.
  bool is_gpu_compositing_disabled_ = false;

  // Utility class to provide GPU functionalities to media.
  // TODO(dcastagna): This should be just one scoped_ptr once
  // http://crbug.com/580386 is fixed.
  // NOTE(dcastagna): At worst this accumulates a few bytes per context lost.
  std::vector<std::unique_ptr<GpuVideoAcceleratorFactoriesImpl>> gpu_factories_;

  // Thread for running multimedia operations (e.g., video decoding).
  std::unique_ptr<base::Thread> media_thread_;

  // Will point to appropriate task runner after initialization,
  // regardless of whether |compositor_thread_| is overriden.
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;

  // Task to run the VideoFrameCompositor on.
  scoped_refptr<base::SingleThreadTaskRunner>
      video_frame_compositor_task_runner_;

  // Pool of workers used for raster operations (e.g., tile rasterization).
  scoped_refptr<CategorizedWorkerPool> categorized_worker_pool_;

#if defined(OS_ANDROID)
  scoped_refptr<StreamTextureFactory> stream_texture_factory_;
#endif

  scoped_refptr<ws::ContextProviderCommandBuffer> shared_main_thread_contexts_;

  base::ObserverList<RenderThreadObserver>::Unchecked observers_;

  scoped_refptr<viz::ContextProvider> video_frame_compositor_context_provider_;

  scoped_refptr<viz::RasterContextProvider> shared_worker_context_provider_;

  std::unique_ptr<AudioRendererMixerManager> audio_renderer_mixer_manager_;

  HistogramCustomizer histogram_customizer_;

  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  // Created in response to EnableV8LowMemoryMode(), this manages V8's
  // memory saving mode.
  std::unique_ptr<LowMemoryModeController> low_memory_mode_controller_;

  std::unique_ptr<ws::Gpu> gpu_;

  scoped_refptr<base::SingleThreadTaskRunner>
      main_thread_compositor_task_runner_;

  // Compositor settings.
  bool is_gpu_rasterization_forced_;
  int gpu_rasterization_msaa_sample_count_;
  bool is_lcd_text_enabled_;
  bool is_zero_copy_enabled_;
  bool is_gpu_memory_buffer_compositor_resources_enabled_;
  bool is_partial_raster_enabled_;
  bool is_elastic_overscroll_enabled_;
  bool is_zoom_for_dsf_enabled_;
  bool is_threaded_animation_enabled_;
  bool is_scroll_animator_enabled_;

  // Target rendering ColorSpace.
  gfx::ColorSpace rendering_color_space_;

  class PendingFrameCreate : public base::RefCounted<PendingFrameCreate> {
   public:
    PendingFrameCreate(const service_manager::BindSourceInfo& source_info,
                       int routing_id,
                       mojom::FrameRequest frame_request);

    const service_manager::BindSourceInfo& browser_info() const {
      return browser_info_;
    }
    mojom::FrameRequest TakeFrameRequest() { return std::move(frame_request_); }

   private:
    friend class base::RefCounted<PendingFrameCreate>;

    ~PendingFrameCreate();

    // Mojo error handler.
    void OnConnectionError();

    service_manager::BindSourceInfo browser_info_;
    int routing_id_;
    mojom::FrameRequest frame_request_;
  };

  using PendingFrameCreateMap =
      std::map<int, scoped_refptr<PendingFrameCreate>>;
  PendingFrameCreateMap pending_frame_creates_;

  blink::mojom::StoragePartitionServicePtr storage_partition_service_;
  mojom::RendererHostAssociatedPtr renderer_host_;

  blink::AssociatedInterfaceRegistry associated_interfaces_;

  mojo::AssociatedBinding<mojom::Renderer> renderer_binding_;

  mojom::RenderFrameMessageFilterAssociatedPtr render_frame_message_filter_;
  mojom::RenderMessageFilterAssociatedPtr render_message_filter_;

  RendererMemoryMetrics purge_and_suspend_memory_metrics_;
  bool needs_to_record_first_active_paint_;
  base::TimeTicks was_backgrounded_time_;
  int process_foregrounded_count_;
  bool online_status_ = true;

  int32_t client_id_;

  mojom::FrameSinkProviderPtr frame_sink_provider_;

  // A mojo connection to the CompositingModeReporter service.
  viz::mojom::CompositingModeReporterPtr compositing_mode_reporter_;
  // The class is a CompositingModeWatcher, which is bound to mojo through
  // this member.
  mojo::Binding<viz::mojom::CompositingModeWatcher>
      compositing_mode_watcher_binding_;

  base::WeakPtrFactory<RenderThreadImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderThreadImpl);
};

#if defined(COMPILER_MSVC)
#pragma warning(pop)
#endif

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_THREAD_IMPL_H_
