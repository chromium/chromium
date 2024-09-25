// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_THREAD_IMPL_H_
#define CONTENT_RENDERER_RENDER_THREAD_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/clang_profiling_buildflags.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/gtest_prod_util.h"
#include "base/memory/discardable_memory_allocator.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/ref_counted.h"
#include "base/memory/structured_shared_memory.h"
#include "base/metrics/user_metrics_action.h"
#include "base/observer_list.h"
#include "base/process/process.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "cc/tiles/gpu_image_decode_cache.h"
#include "content/child/child_thread_impl.h"
#include "content/common/agent_scheduling_group.mojom.h"
#include "content/common/content_export.h"
#include "content/common/frame.mojom.h"
#include "content/common/render_message_filter.mojom.h"
#include "content/common/renderer.mojom.h"
#include "content/common/renderer_host.mojom.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/discardable_memory_utils.h"
#include "content/renderer/media/codec_factory.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "ipc/ipc_sync_channel.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/network_change_notifier.h"
#include "net/nqe/effective_connection_type.h"
#include "services/viz/public/mojom/compositing/compositing_mode_watcher.mojom.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/performance/performance_scenarios.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "third_party/blink/public/mojom/origin_trials/origin_trials_settings.mojom-forward.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage_worklet_service.mojom.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/platform/url_loader_throttle_provider.h"
#include "third_party/blink/public/platform/web_connection_type.h"
#include "ui/gfx/native_widget_types.h"

namespace blink {
class WebVideoCaptureImplManager;
}

namespace base {
class SingleThreadTaskRunner;
class Thread;
class WaitableEvent;
}

namespace cc {
class RasterContextProviderWrapper;
class RasterDarkModeFilter;
}  // namespace cc

namespace gpu {
class GpuChannelHost;
class ClientSharedImageInterface;
}

namespace media {
class GpuVideoAcceleratorFactories;
}

namespace mojo {
class BinderMap;
}

namespace viz {
class ContextProviderCommandBuffer;
class Gpu;
class RasterContextProvider;
}  // namespace viz

namespace content {
class AgentSchedulingGroup;
class GpuVideoAcceleratorFactoriesImpl;
class RenderFrameImpl;
class RenderThreadObserver;
class RendererBlinkPlatformImpl;
class VariationsRenderThreadObserver;

#if BUILDFLAG(IS_ANDROID)
class StreamTextureFactory;
#endif

#if BUILDFLAG(IS_WIN)
class DCOMPTextureFactory;
class OverlayStateServiceProvider;
class OverlayStateServiceProviderImpl;
#endif

// The RenderThreadImpl class represents the main thread, where `blink::WebView`
// instances live.  Most of the communication occurs in the form of mojo IPC
// messages, however there is still some legacy IPC messages.  They are
// routed to the RenderThread according to the routing IDs of the messages.
// The routing IDs correspond to `RenderFrameImpl` instances.
class CONTENT_EXPORT RenderThreadImpl
    : public RenderThread,
      public ChildThreadImpl,
      public mojom::Renderer,
      public viz::mojom::CompositingModeWatcher {
 public:
  static RenderThreadImpl* current();

  // Returns the task runner for the main thread where the RenderThread lives.
  static scoped_refptr<base::SingleThreadTaskRunner>
  DeprecatedGetMainTaskRunner();

  RenderThreadImpl(
      base::RepeatingClosure quit_closure,
      std::unique_ptr<blink::scheduler::WebThreadScheduler> scheduler);
  RenderThreadImpl(
      const InProcessChildThreadParams& params,
      int32_t client_id,
      std::unique_ptr<blink::scheduler::WebThreadScheduler> scheduler);

  RenderThreadImpl(const RenderThreadImpl&) = delete;
  RenderThreadImpl& operator=(const RenderThreadImpl&) = delete;

  ~RenderThreadImpl() override;
  void Shutdown() override;
  bool ShouldBeDestroyed() override;

  // When initializing WebKit, ensure that any schemes needed for the content
  // module are registered properly.  Static to allow sharing with tests.
  static void RegisterSchemes();

  // RenderThread implementation:
  IPC::SyncChannel* GetChannel() override;
  std::string GetLocale() override;

#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
  IPC::SyncMessageFilter* GetSyncMessageFilter() override;
  void AddRoute(int32_t routing_id, IPC::Listener* listener) override;
  void AttachTaskRunnerToRoute(
      int32_t routing_id,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;
  void RemoveRoute(int32_t routing_id) override;
  void AddFilter(IPC::MessageFilter* filter) override;
  void RemoveFilter(IPC::MessageFilter* filter) override;
#endif

  bool GenerateFrameRoutingID(int32_t& routing_id,
                              blink::LocalFrameToken& frame_token,
                              base::UnguessableToken& devtools_frame_token,
                              blink::DocumentToken& document_token) override;
  void AddObserver(RenderThreadObserver* observer) override;
  void RemoveObserver(RenderThreadObserver* observer) override;
  int PostTaskToAllWebWorkers(base::RepeatingClosure closure) override;
  base::WaitableEvent* GetShutdownEvent() override;
  int32_t GetClientId() override;
  void SetRendererProcessType(
      blink::scheduler::WebRendererProcessType type) override;
  blink::WebString GetUserAgent() override;
  const blink::UserAgentMetadata& GetUserAgentMetadata() override;
  void WriteIntoTrace(
      perfetto::TracedProto<perfetto::protos::pbzero::RenderProcessHost> proto)
      override;

  // IPC::Listener implementation via ChildThreadImpl:
  void OnAssociatedInterfaceRequest(
      const std::string& name,
      mojo::ScopedInterfaceEndpointHandle handle) override;

  blink::scheduler::WebThreadScheduler* GetWebMainThreadScheduler();
  bool IsLcdTextEnabled();
  bool IsElasticOverscrollEnabled();
  bool IsScrollAnimatorEnabled();

  // TODO(crbug.com/40142495): The `enable_scroll_animator` flag is currently
  // being passed as part of `CreateViewParams`, despite it looking like a
  // global setting. It should probably be moved to some `mojom::Renderer` API
  // and this method should be removed.
  void SetScrollAnimatorEnabled(bool enable_scroll_animator,
                                base::PassKey<AgentSchedulingGroup>);

  bool IsThreadedAnimationEnabled();

  // viz::mojom::CompositingModeWatcher implementation.
  void CompositingModeFallbackToSoftware() override;

  // Whether gpu compositing is being used or is disabled for software
  // compositing. Clients of the compositor should give resources that match
  // the appropriate mode.
  bool IsGpuCompositingDisabled() const { return is_gpu_compositing_disabled_; }

  bool IsGpuRemoteDisconnected();

  // Synchronously establish a channel to the GPU plugin if not previously
  // established or if it has been lost (for example if the GPU plugin crashed).
  // If there is a pending asynchronous request, it will be completed by the
  // time this routine returns.
  scoped_refptr<gpu::GpuChannelHost> EstablishGpuChannelSync();

  // Same as above, but asynchronous.
  using EstablishGpuChannelCallback =
      base::OnceCallback<void(scoped_refptr<gpu::GpuChannelHost>)>;
  void EstablishGpuChannel(EstablishGpuChannelCallback callback);

  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager();

  blink::AssociatedInterfaceRegistry* GetAssociatedInterfaceRegistry();

  base::DiscardableMemoryAllocator* GetDiscardableMemoryAllocatorForTest()
      const {
    return discardable_memory_allocator_.get();
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

  const std::vector<std::string> cors_exempt_header_list() const {
    return cors_exempt_header_list_;
  }

  blink::URLLoaderThrottleProvider* url_loader_throttle_provider() const {
    return url_loader_throttle_provider_.get();
  }

#if BUILDFLAG(IS_ANDROID)
  scoped_refptr<StreamTextureFactory> GetStreamTexureFactory();
  bool EnableStreamTextureCopy();
#endif

#if BUILDFLAG(IS_WIN)
  scoped_refptr<DCOMPTextureFactory> GetDCOMPTextureFactory();
  // The OverlayStateService is only available where Media Foundation for
  // clear is supported, otherwise GetOverlayStateServiceProvider will return
  // nullptr.
  OverlayStateServiceProvider* GetOverlayStateServiceProvider();
#endif

  blink::WebVideoCaptureImplManager* video_capture_impl_manager() const {
    return vc_manager_.get();
  }

  // Get the GPU channel. Returns NULL if the channel is not established or
  // has been lost.
  gpu::GpuChannelHost* GetGpuChannel();

  // Returns the sequence on which media operations should be run. Must be
  // called on the renderer's main thread.
  scoped_refptr<base::SequencedTaskRunner> GetMediaSequencedTaskRunner();

  // Creates a ContextProvider if yet created, and returns it to be used for
  // video frame compositing. The ContextProvider given as an argument is
  // one that has been lost, and is a hint to the RenderThreadImpl to clear
  // it's |video_frame_compositor_context_provider_| if it matches.
  scoped_refptr<viz::RasterContextProvider>
      GetVideoFrameCompositorContextProvider(
          scoped_refptr<viz::RasterContextProvider>);

  scoped_refptr<gpu::ClientSharedImageInterface>
  GetRenderThreadSharedImageInterface();

  // Returns a worker context provider that will be bound on the compositor
  // thread.
  scoped_refptr<cc::RasterContextProviderWrapper>
  SharedCompositorWorkerContextProvider(
      cc::RasterDarkModeFilter* dark_mode_filter);

  media::GpuVideoAcceleratorFactories* GetGpuFactories();

  scoped_refptr<viz::ContextProviderCommandBuffer>
  SharedMainThreadContextProvider();

  // For producing custom V8 histograms. Custom histograms are produced if all
  // `blink::WebView`s share the same host, and the host is in the pre-specified
  // set of hosts we want to produce custom diagrams for. The name for a custom
  // diagram is the name of the corresponding generic diagram plus a
  // host-specific suffix.
  class CONTENT_EXPORT HistogramCustomizer {
   public:
    HistogramCustomizer();

    HistogramCustomizer(const HistogramCustomizer&) = delete;
    HistogramCustomizer& operator=(const HistogramCustomizer&) = delete;

    ~HistogramCustomizer();

    // Called when a top frame of a `blink::WebView` navigates. This function
    // updates RenderThreadImpl's information about whether all
    // `blink::WebView`s are displaying a page from the same host. |host| is the
    // host where a `blink::WebView` navigated, and |view_count| is the number
    // of `blink::WebView`s in this process.
    void RenderViewNavigatedToHost(const std::string& host, size_t view_count);

    // Used for customizing some histograms if all `blink::WebView`s share the
    // same host. Returns the current custom histogram name to use for
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
    // `blink::WebView`'s share (if any). If there is no common host, this
    // function is called with an empty string.
    void SetCommonHost(const std::string& host);

    // The current common host of the `blink::WebView`s; empty string if there
    // is no common host.
    std::string common_host_;
    // The corresponding suffix.
    std::string common_host_histogram_suffix_;
    // Set of histograms for which we want to produce a custom histogram if
    // possible.
    std::set<std::string> custom_histograms_;
  };

  HistogramCustomizer* histogram_customizer() {
    return &histogram_customizer_;
  }

  mojom::RendererHost* GetRendererHost();

  // Sets the current pipeline rendering color space.
  void SetRenderingColorSpace(const gfx::ColorSpace& color_space);

  gfx::ColorSpace GetRenderingColorSpace();

  // The time the run loop started for this thread.
  base::TimeTicks run_loop_start_time() const { return run_loop_start_time_; }

  void set_run_loop_start_time(base::TimeTicks run_loop_start_time) {
    run_loop_start_time_ = run_loop_start_time;
  }

#if BUILDFLAG(IS_ANDROID)
  // Provide private memory footprint for browser process.
  void SetPrivateMemoryFootprint(uint64_t private_memory_footprint_bytes);
#endif

 private:
  FRIEND_TEST_ALL_PREFIXES(RenderThreadImplBrowserTest,
                           TransferSharedMemoryRegions);
  friend class RenderThreadImplBrowserTest;
  friend class AgentSchedulingGroup;

  void OnProcessFinalRelease() override;
  // IPC::Listener
  void OnChannelError() override;

  // ChildThread
#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
  bool OnControlMessageReceived(const IPC::Message& msg) override;
#endif
  void RecordAction(const base::UserMetricsAction& action) override;
  void RecordComputedAction(const std::string& action) override;

#if BUILDFLAG(IS_ANDROID)
  // ChildThreadImpl
  void OnMemoryPressureFromBrowserReceived(
      base::MemoryPressureListener::MemoryPressureLevel level) override;
#endif
  void SetBatterySaverMode(bool battery_saver_mode_enabled) override;

  bool IsMainThread();

  void Init();
  void InitializeCompositorThread();
  void InitializeWebKit(mojo::BinderMap* binders);

  // mojom::Renderer:
  void CreateAgentSchedulingGroup(
      mojo::PendingReceiver<IPC::mojom::ChannelBootstrap> bootstrap) override;
  void CreateAssociatedAgentSchedulingGroup(
      mojo::PendingAssociatedReceiver<mojom::AgentSchedulingGroup>
          agent_scheduling_group) override;
  void TransferSharedMemoryRegions(
      base::ReadOnlySharedMemoryRegion last_foreground_time_region,
      base::ReadOnlySharedMemoryRegion performance_scenario_region,
      base::ReadOnlySharedMemoryRegion global_performance_scenario_region)
      override;
  void OnNetworkConnectionChanged(
      net::NetworkChangeNotifier::ConnectionType type,
      double max_bandwidth_mbps) override;
  void OnNetworkQualityChanged(net::EffectiveConnectionType type,
                               base::TimeDelta http_rtt,
                               base::TimeDelta transport_rtt,
                               double bandwidth_kbps) override;
  void SetWebKitSharedTimersSuspended(bool suspend) override;
  void InitializeRenderer(
      const std::string& user_agent,
      const blink::UserAgentMetadata& user_agent_metadata,
      const std::vector<std::string>& cors_exempt_header_list,
      blink::mojom::OriginTrialsSettingsPtr origin_trial_settings) override;
  void UpdateScrollbarTheme(
      mojom::UpdateScrollbarThemeParamsPtr params) override;
  void OnSystemColorsChanged(int32_t aqua_color_variant) override;
  void UpdateSystemColorInfo(
      mojom::UpdateSystemColorInfoParamsPtr params) override;
  void PurgePluginListCache(bool reload_pages) override;
  void PurgeResourceCache(PurgeResourceCacheCallback callback) override;
  void SetProcessState(base::Process::Priority priority,
                       mojom::RenderProcessVisibleState visible_state) override;
  void SetIsLockedToSite() override;
#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
  void WriteClangProfilingProfile(
      WriteClangProfilingProfileCallback callback) override;
#endif
  void SetIsCrossOriginIsolated(bool value) override;
  void SetIsWebSecurityDisabled(bool value) override;
  void SetIsIsolatedContext(bool value) override;
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

  bool RendererIsHidden() const;
  void OnRendererHidden();
  void OnRendererVisible();

  bool RendererIsBackgrounded() const;
  void OnRendererBackgrounded();
  void OnRendererForegrounded();

  void ReleaseFreeMemory();

  void OnSyncMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

  void OnRendererInterfaceReceiver(
      mojo::PendingAssociatedReceiver<mojom::Renderer> receiver);

  std::unique_ptr<CodecFactory> CreateMediaCodecFactory(
      scoped_refptr<viz::ContextProviderCommandBuffer> context_provider,
      bool enable_video_decode_accelerator,
      bool enable_video_encode_accelerator);

  mojom::RenderMessageFilter* render_message_filter();
  void RequestNewItemsForFrameRoutingCache();
  void PopulateFrameRoutingCacheWithItems(
      std::vector<mojom::FrameRoutingInfoPtr> infos);

  scoped_refptr<discardable_memory::ClientDiscardableSharedMemoryManager>
      discardable_memory_allocator_;

  // These objects live solely on the render thread.
  std::unique_ptr<blink::scheduler::WebThreadScheduler> main_thread_scheduler_;
  std::unique_ptr<RendererBlinkPlatformImpl> blink_platform_impl_;
  std::unique_ptr<blink::URLLoaderThrottleProvider>
      url_loader_throttle_provider_;

  std::vector<std::string> cors_exempt_header_list_;

  // Used on the render thread.
  std::unique_ptr<blink::WebVideoCaptureImplManager> vc_manager_;

  // Used to keep track of the renderer's backgrounded and visibility state.
  // Updated via an IPC from the browser process. If nullopt, the browser
  // process has yet to send an update and the state is unknown.
  std::optional<base::Process::Priority> process_priority_;
  std::optional<mojom::RenderProcessVisibleState> visible_state_;

  // A read-only mapping of a std::atomic<base::TimeTicks> set to
  // TimeTicks::Now() by RenderProcessHostImpl when this process is foregrounded
  // and back to a null TimeTicks when it's backgrounded. Used to track the
  // exact state of this process without relying on IPC (which can itself be
  // delayed) for use cases that require that precision.
  std::optional<base::AtomicSharedMemory<base::TimeTicks>::ReadOnlyMapping>
      last_foreground_time_mapping_;

  std::optional<blink::performance_scenarios::ScopedReadOnlyScenarioMemory>
      performance_scenario_memory_;
  std::optional<blink::performance_scenarios::ScopedReadOnlyScenarioMemory>
      global_performance_scenario_memory_;

  blink::WebString user_agent_;
  blink::UserAgentMetadata user_agent_metadata_;

  // Sticky once true, indicates that compositing is done without Gpu, so
  // resources given to the compositor or to the viz service should be
  // software-based.
  bool is_gpu_compositing_disabled_ = false;

  // Utility class to provide GPU functionalities to media.
  // TODO(dcastagna): This should be just one scoped_ptr once
  // http://crbug.com/580386 is fixed.
  // NOTE(dcastagna): At worst this accumulates a few bytes per context lost.
  std::vector<std::unique_ptr<GpuVideoAcceleratorFactoriesImpl>> gpu_factories_;

  // Thread or sequenced task runner (depending on
  // kBlinkMediaIsPooledSequencedTaskRunner) for running multimedia operations
  // (e.g., video decoding). Exactly one of these is in use after
  // GetMediaSequencedTaskRunner has been called.
  std::unique_ptr<base::Thread> media_thread_;
  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;

  // Will point to appropriate task runner after initialization,
  // regardless of whether |compositor_thread_| is overriden.
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;

  // Thread to run the VideoFrameCompositor on.
  std::unique_ptr<base::Thread> video_frame_compositor_thread_;

#if BUILDFLAG(IS_ANDROID)
  scoped_refptr<StreamTextureFactory> stream_texture_factory_;
#endif

#if BUILDFLAG(IS_WIN)
  scoped_refptr<DCOMPTextureFactory> dcomp_texture_factory_;
  std::unique_ptr<OverlayStateServiceProviderImpl>
      overlay_state_service_provider_;
#endif

  scoped_refptr<viz::ContextProviderCommandBuffer> shared_main_thread_contexts_;

  base::ObserverList<RenderThreadObserver>::Unchecked observers_;

  scoped_refptr<viz::RasterContextProvider>
      video_frame_compositor_context_provider_;

  scoped_refptr<cc::RasterContextProviderWrapper>
      shared_worker_context_provider_wrapper_;

  scoped_refptr<gpu::ClientSharedImageInterface> shared_image_interface_;

  HistogramCustomizer histogram_customizer_;

  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  std::unique_ptr<viz::Gpu> gpu_;

  std::unique_ptr<VariationsRenderThreadObserver> variations_observer_;

  // Compositor settings.
  bool is_lcd_text_enabled_;
  bool is_partial_raster_enabled_;
  bool is_elastic_overscroll_enabled_;
  bool is_threaded_animation_enabled_;
  bool is_scroll_animator_enabled_;

  // Target rendering ColorSpace.
  gfx::ColorSpace rendering_color_space_;

  mojo::AssociatedRemote<mojom::RendererHost> renderer_host_;

  blink::AssociatedInterfaceRegistry associated_interfaces_;

  mojo::AssociatedReceiver<mojom::Renderer> renderer_receiver_{this};

  mojo::Remote<mojom::RenderMessageFilter> render_message_filter_;

  std::set<std::unique_ptr<AgentSchedulingGroup>, base::UniquePtrComparator>
      agent_scheduling_groups_;

  int process_foregrounded_count_;

  int32_t client_id_;

  bool is_context_result_fatal_ = false;

  // A mojo connection to the CompositingModeReporter service.
  mojo::Remote<viz::mojom::CompositingModeReporter> compositing_mode_reporter_;
  // The class is a CompositingModeWatcher, which is bound to mojo through
  // this member.
  mojo::Receiver<viz::mojom::CompositingModeWatcher>
      compositing_mode_watcher_receiver_{this};

  // Tracks the time the run loop started for this thread.
  base::TimeTicks run_loop_start_time_;

  // A small cache of pending frame routing IDs/tokens so we do not need to make
  // a synchronous IPC call to retrieve one most of the time. If the cache is
  // empty a synchronous IPC call will be made. Once the cache only has two
  // items an asynchronous request to populate it will also be made.
  std::deque<mojom::FrameRoutingInfoPtr> cached_frame_routing_;

  // Keep track of it we have requested items or not as we do not want to fire
  // off only one asynchronous request.
  bool cached_items_requested_ = false;
  bool use_cached_routing_table_ = false;

  std::optional<base::ThreadPoolInstance::ScopedRestrictedTasks>
      restrict_thread_pool_;

  base::WeakPtrFactory<RenderThreadImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_THREAD_IMPL_H_
