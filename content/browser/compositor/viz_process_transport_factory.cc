// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/compositor/viz_process_transport_factory.h"

#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/mojo_embedder/async_layer_tree_frame_sink.h"
#include "cc/raster/single_thread_task_graph_runner.h"
#include "cc/tiles/image_decode_cache_utils.h"
#include "cc/trees/raster_context_provider_wrapper.h"
#include "components/viz/common/features.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/switches.h"
#include "components/viz/host/compositing_mode_reporter_impl.h"
#include "components/viz/host/gpu_host_impl.h"
#include "components/viz/host/host_display_client.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/host/renderer_settings_creation.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/gpu_stream_constants.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/viz/privileged/mojom/compositing/display_private.mojom.h"
#include "services/viz/privileged/mojom/compositing/external_begin_frame_controller.mojom.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/base/ui_base_features.h"

#if BUILDFLAG(IS_WIN)
#include "ui/gfx/win/rendering_window_manager.h"
#endif

namespace content {
namespace {

// The client id for the browser process. It must not conflict with any
// child process client id.
constexpr uint32_t kBrowserClientId = 0u;

scoped_refptr<viz::ContextProviderCommandBuffer> CreateContextProvider(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
    bool supports_locking,
    bool supports_gpu_rasterization,
    viz::command_buffer_metrics::ContextType type) {
  constexpr bool kAutomaticFlushes = false;

  gpu::ContextCreationAttribs attributes;
  attributes.bind_generates_resource = false;
  attributes.lose_context_when_out_of_memory = true;
  attributes.enable_gles2_interface = false;
  attributes.enable_raster_interface = true;
  attributes.enable_oop_rasterization = supports_gpu_rasterization;

  gpu::SharedMemoryLimits memory_limits =
      gpu::SharedMemoryLimits::ForDisplayCompositor();

  GURL url("chrome://gpu/VizProcessTransportFactory::CreateContextProvider");
  return base::MakeRefCounted<viz::ContextProviderCommandBuffer>(
      std::move(gpu_channel_host), kGpuStreamIdDefault, kGpuStreamPriorityUI,
      gpu::kNullSurfaceHandle, std::move(url), kAutomaticFlushes,
      supports_locking, memory_limits, attributes, type);
}

bool IsContextLost(viz::RasterContextProvider* context_provider) {
  return context_provider->RasterInterface()->GetGraphicsResetStatusKHR() !=
         GL_NO_ERROR;
}

bool IsWorkerContextLost(viz::RasterContextProvider* context_provider) {
  viz::RasterContextProvider::ScopedRasterContextLock lock(context_provider);
  return lock.RasterInterface()->GetGraphicsResetStatusKHR() != GL_NO_ERROR;
}

class HostDisplayClient : public viz::HostDisplayClient {
 public:
  explicit HostDisplayClient(ui::Compositor* compositor)
      : viz::HostDisplayClient(compositor->widget()), compositor_(compositor) {}
  ~HostDisplayClient() override = default;
  HostDisplayClient(const HostDisplayClient&) = delete;
  HostDisplayClient& operator=(const HostDisplayClient&) = delete;

  // viz::HostDisplayClient:
#if BUILDFLAG(IS_LINUX) && BUILDFLAG(IS_OZONE_X11)
  void DidCompleteSwapWithNewSize(const gfx::Size& size) override {
    compositor_->OnCompleteSwapWithNewSize(size);
  }
#endif  // BUILDFLAG(IS_LINUX) && BUILDFLAG(IS_OZONE_X11)

#if BUILDFLAG(IS_WIN)
  void AddChildWindowToBrowser(gpu::SurfaceHandle child_window) override {
    content::GpuProcessHost* gpu_process_host = content::GpuProcessHost::Get(
        GPU_PROCESS_KIND_SANDBOXED, /*force_create=*/false);
    if (!gpu_process_host) {
      return;
    }
    gpu_process_host->gpu_host()->AddChildWindow(widget(), child_window);
  }
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetPreferredRefreshRate(float refresh_rate) override {
    compositor_->OnSetPreferredRefreshRate(refresh_rate);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

 private:
  [[maybe_unused]] const raw_ptr<ui::Compositor> compositor_;
};

}  // namespace

VizProcessTransportFactory::VizProcessTransportFactory(
    gpu::GpuChannelEstablishFactory* gpu_channel_establish_factory,
    scoped_refptr<base::SingleThreadTaskRunner> resize_task_runner,
    viz::CompositingModeReporterImpl* compositing_mode_reporter)
    : gpu_channel_establish_factory_(gpu_channel_establish_factory),
      compositing_mode_reporter_(compositing_mode_reporter),
      task_graph_runner_(std::make_unique<cc::SingleThreadTaskGraphRunner>()),
      frame_sink_id_allocator_(kBrowserClientId),
      host_frame_sink_manager_(
          BrowserMainLoop::GetInstance()->host_frame_sink_manager()),
      resize_task_runner_(resize_task_runner) {
  DCHECK(gpu_channel_establish_factory_);
  task_graph_runner_->Start("CompositorTileWorker1",
                            base::SimpleThread::Options());
  GetHostFrameSinkManager()->SetConnectionLostCallback(
      base::BindRepeating(&VizProcessTransportFactory::OnGpuProcessLost,
                          weak_ptr_factory_.GetWeakPtr()));

  if (GpuDataManagerImpl::GetInstance()->IsGpuCompositingDisabled()) {
    DisableGpuCompositing(nullptr);
  }
}

VizProcessTransportFactory::~VizProcessTransportFactory() {
  task_graph_runner_->Shutdown();
}

void VizProcessTransportFactory::ConnectHostFrameSinkManager() {
  mojo::PendingRemote<viz::mojom::FrameSinkManager> frame_sink_manager;
  mojo::PendingReceiver<viz::mojom::FrameSinkManager>
      frame_sink_manager_receiver =
          frame_sink_manager.InitWithNewPipeAndPassReceiver();
  mojo::PendingRemote<viz::mojom::FrameSinkManagerClient>
      frame_sink_manager_client;
  mojo::PendingReceiver<viz::mojom::FrameSinkManagerClient>
      frame_sink_manager_client_receiver =
          frame_sink_manager_client.InitWithNewPipeAndPassReceiver();

  // Setup HostFrameSinkManager with interface endpoints.
  GetHostFrameSinkManager()->BindAndSetManager(
      std::move(frame_sink_manager_client_receiver), resize_task_runner_,
      std::move(frame_sink_manager));

  // There should always be a GpuProcessHost instance, and GPU process,
  // for running the compositor thread. The exception is during shutdown
  // the GPU process won't be restarted and GpuProcessHost::Get() can
  // return null.
  auto* gpu_process_host = GpuProcessHost::Get();
  if (gpu_process_host) {
    gpu_process_host->gpu_host()->ConnectFrameSinkManager(
        std::move(frame_sink_manager_receiver),
        std::move(frame_sink_manager_client),
        GetHostFrameSinkManager()->debug_renderer_settings());
  }
}

void VizProcessTransportFactory::CreateLayerTreeFrameSink(
    base::WeakPtr<ui::Compositor> compositor) {
#if BUILDFLAG(IS_WIN)
  gfx::RenderingWindowManager::GetInstance()->UnregisterParent(
      compositor->widget());
#endif

  gpu_channel_establish_factory_->EstablishGpuChannel(
      base::BindOnce(&VizProcessTransportFactory::OnEstablishedGpuChannel,
                     weak_ptr_factory_.GetWeakPtr(), compositor));
}

scoped_refptr<viz::RasterContextProvider>
VizProcessTransportFactory::SharedMainThreadRasterContextProvider() {
  if (is_gpu_compositing_disabled_) {
    return nullptr;
  }

  if (main_context_provider_ && IsContextLost(main_context_provider_.get())) {
    main_context_provider_.reset();
  }

  if (!main_context_provider_) {
    auto context_result = gpu::ContextResult::kTransientFailure;
    while (context_result == gpu::ContextResult::kTransientFailure) {
      context_result = TryCreateContextsForGpuCompositing(
          gpu_channel_establish_factory_->EstablishGpuChannelSync());

      if (gpu::IsFatalOrSurfaceFailure(context_result))
        DisableGpuCompositing(nullptr);
    }
    // On kFatalFailure or kSurfaceFailure, |main_context_provider_| will be
    // null.
  }

  return main_context_provider_;
}

void VizProcessTransportFactory::RemoveCompositor(ui::Compositor* compositor) {
#if BUILDFLAG(IS_WIN)
  gfx::RenderingWindowManager::GetInstance()->UnregisterParent(
      compositor->widget());
#endif

  compositor_data_map_.erase(compositor);
}

gpu::GpuMemoryBufferManager*
VizProcessTransportFactory::GetGpuMemoryBufferManager() {
  return gpu_channel_establish_factory_->GetGpuMemoryBufferManager();
}

cc::TaskGraphRunner* VizProcessTransportFactory::GetTaskGraphRunner() {
  return task_graph_runner_.get();
}

viz::FrameSinkId VizProcessTransportFactory::AllocateFrameSinkId() {
  return frame_sink_id_allocator_.NextFrameSinkId();
}

viz::SubtreeCaptureId VizProcessTransportFactory::AllocateSubtreeCaptureId() {
  return subtree_capture_id_allocator_.NextSubtreeCaptureId();
}

viz::HostFrameSinkManager*
VizProcessTransportFactory::GetHostFrameSinkManager() {
  return host_frame_sink_manager_;
}

void VizProcessTransportFactory::DisableGpuCompositing() {
  if (!is_gpu_compositing_disabled_)
    DisableGpuCompositing(nullptr);
}

ui::ContextFactory* VizProcessTransportFactory::GetContextFactory() {
  return this;
}

void VizProcessTransportFactory::DisableGpuCompositing(
    ui::Compositor* guilty_compositor) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // A fatal error has occurred and we can't fall back to software compositing
  // on CrOS. These can be unrecoverable hardware errors, or bugs that should
  // not happen. Crash the browser process to reset everything.
  LOG(FATAL) << "Software compositing fallback is unavailable. Goodbye.";
#else
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableSoftwareCompositingFallback)) {
    // Some tests only want to run with a functional GPU Process. Fail out here
    // rather than falling back to software compositing and silently passing.
    LOG(FATAL) << "Software compositing fallback is unavailable. Goodbye.";
  }

  DVLOG(1) << "Switching to software compositing.";

  is_gpu_compositing_disabled_ = true;

  // Change the result of GpuDataManagerImpl::IsGpuCompositingDisabled() before
  // notifying anything.
  GpuDataManagerImpl::GetInstance()->SetGpuCompositingDisabled();

  compositing_mode_reporter_->SetUsingSoftwareCompositing();

  // Drop our reference on the gpu contexts for the compositors.
  worker_context_provider_wrapper_.reset();
  main_context_provider_.reset();

  // ReleaseAcceleratedWidget() removes an entry from |compositor_data_map_|,
  // so first copy the compositors to a new set.
  base::flat_set<ui::Compositor*> all_compositors;
  all_compositors.reserve(compositor_data_map_.size());
  for (auto& pair : compositor_data_map_)
    all_compositors.insert(pair.first);

  // Remove the FrameSink from every compositor that needs to fall back to
  // software compositing.
  for (auto* compositor : all_compositors) {
    // The |guilty_compositor| is in the process of setting up its FrameSink
    // so removing it from |compositor_data_map_| would be both pointless and
    // the cause of a crash.
    // Compositors with force_software_compositor() do not follow the global
    // compositing mode, so they do not need to be changed.
    if (compositor == guilty_compositor ||
        compositor->force_software_compositor())
      continue;

    // Compositor expects to be not visible when releasing its FrameSink.
    bool visible = compositor->IsVisible();
    compositor->SetVisible(false);
    gfx::AcceleratedWidget widget = compositor->ReleaseAcceleratedWidget();
    compositor->SetAcceleratedWidget(widget);
    if (visible)
      compositor->SetVisible(true);
  }
#endif
}

void VizProcessTransportFactory::OnGpuProcessLost() {
  // Reconnect HostFrameSinkManager to new GPU process.
  ConnectHostFrameSinkManager();
}

void VizProcessTransportFactory::OnEstablishedGpuChannel(
    base::WeakPtr<ui::Compositor> compositor_weak_ptr,
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host) {
  ui::Compositor* compositor = compositor_weak_ptr.get();
  if (!compositor)
    return;

  bool gpu_compositing =
      !is_gpu_compositing_disabled_ && !compositor->force_software_compositor();

  if (gpu_compositing) {
    auto context_result = TryCreateContextsForGpuCompositing(gpu_channel_host);
    if (context_result == gpu::ContextResult::kTransientFailure) {
      // Get a new GpuChannelHost and retry context creation.
      gpu_channel_establish_factory_->EstablishGpuChannel(
          base::BindOnce(&VizProcessTransportFactory::OnEstablishedGpuChannel,
                         weak_ptr_factory_.GetWeakPtr(), compositor_weak_ptr));
      return;
    } else if (gpu::IsFatalOrSurfaceFailure(context_result)) {
      DisableGpuCompositing(compositor);
      gpu_compositing = false;
    }
  }

  scoped_refptr<viz::RasterContextProvider> context_provider;
  scoped_refptr<cc::RasterContextProviderWrapper>
      worker_context_provider_wrapper;
  if (gpu_compositing) {
    // Only pass the contexts to the compositor if it will use gpu compositing.
    context_provider = main_context_provider_;
    worker_context_provider_wrapper = worker_context_provider_wrapper_;
  }

#if BUILDFLAG(IS_WIN)
  gfx::RenderingWindowManager::GetInstance()->RegisterParent(
      compositor->widget());
#endif
  auto& compositor_data = compositor_data_map_[compositor];

  auto root_params = viz::mojom::RootCompositorFrameSinkParams::New();
  // Create interfaces for a root CompositorFrameSink.
  mojo::PendingAssociatedRemote<viz::mojom::CompositorFrameSink> sink_remote;
  root_params->compositor_frame_sink =
      sink_remote.InitWithNewEndpointAndPassReceiver();
  mojo::PendingReceiver<viz::mojom::CompositorFrameSinkClient> client_receiver =
      root_params->compositor_frame_sink_client
          .InitWithNewPipeAndPassReceiver();
  mojo::AssociatedRemote<viz::mojom::DisplayPrivate> display_private;
  root_params->display_private =
      display_private.BindNewEndpointAndPassReceiver();
  compositor_data.display_client =
      std::make_unique<HostDisplayClient>(compositor);
  root_params->display_client =
      compositor_data.display_client->GetBoundRemote(resize_task_runner_);
  mojo::AssociatedRemote<viz::mojom::ExternalBeginFrameController>
      external_begin_frame_controller;
  if (compositor->use_external_begin_frame_control()) {
    root_params->external_begin_frame_controller =
        external_begin_frame_controller.BindNewEndpointAndPassReceiver();
  }

  root_params->frame_sink_id = compositor->frame_sink_id();
#if defined(GPU_SURFACE_HANDLE_IS_ACCELERATED_WINDOW)
  root_params->widget = compositor->widget();
#endif
  root_params->gpu_compositing = gpu_compositing;
  root_params->renderer_settings = viz::CreateRendererSettings();
#if BUILDFLAG(IS_MAC)
  root_params->renderer_settings.display_id = compositor->display_id();
#endif
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableFrameRateLimit))
    root_params->disable_frame_rate_limit = true;

#if BUILDFLAG(IS_WIN)
  const bool using_direct_composition = GpuDataManagerImpl::GetInstance()
                                            ->GetGPUInfo()
                                            .overlay_info.direct_composition;
  // The wait_on_destruction flag governs whether InvalidateFrameSinkId calls
  // DestroyCompositorFrameSink synchronously, thus ensuring that the surface
  // that draws to the HWND gets destroyed before the HWND, itself, gets
  // destroyed.

  // Skipping DestroyCompositorFrameSink is safe when we're using direct
  // composition mode. In DComp mode, we create a child popup HWND (to which we
  // attach a visual tree) and ask the browser process to parent it to its HWND
  // via AddChildWindowToBrowser. Thus, it is safe to delete the parent window.

  // In non-DComp hardware modes, failure to call DestroyCompositorFrameSink
  // leads to a race condition where the HWND can be deleted out from under the
  // GPU process. API calls with the HWND will fail and lead to the GPU process
  // falling back to software mode.

  // CreateRootCompositorFrameSink connects the viz process end of
  // CompositorFrameSink message pipes. The browser compositor may request a new
  // CompositorFrameSink on context loss, which will destroy the existing
  // CompositorFrameSink.
  GetHostFrameSinkManager()->CreateRootCompositorFrameSink(
      std::move(root_params), !using_direct_composition);
#else
  GetHostFrameSinkManager()->CreateRootCompositorFrameSink(
      std::move(root_params));
#endif  // BUILDFLAG(IS_WIN)

  // Create LayerTreeFrameSink with the browser end of CompositorFrameSink.
  cc::mojo_embedder::AsyncLayerTreeFrameSink::InitParams params;
  params.compositor_task_runner = compositor->task_runner();
  params.gpu_memory_buffer_manager =
      compositor->context_factory()
          ? compositor->context_factory()->GetGpuMemoryBufferManager()
          : nullptr;
  params.pipes.compositor_frame_sink_associated_remote = std::move(sink_remote);
  params.pipes.client_receiver = std::move(client_receiver);

  scoped_refptr<gpu::ClientSharedImageInterface> shared_image_interface;
  if (gpu_channel_host) {
    shared_image_interface =
        gpu_channel_host->CreateClientSharedImageInterface();
  }
  auto frame_sink =
      std::make_unique<cc::mojo_embedder::AsyncLayerTreeFrameSink>(
          std::move(context_provider),
          std::move(worker_context_provider_wrapper),
          std::move(shared_image_interface), &params);
  compositor->SetLayerTreeFrameSink(std::move(frame_sink),
                                    std::move(display_private));
  if (compositor->use_external_begin_frame_control()) {
    compositor->SetExternalBeginFrameController(
        std::move(external_begin_frame_controller));
  }

#if BUILDFLAG(IS_WIN)
  // Windows using the ANGLE D3D backend for compositing needs to disable swap
  // on resize to avoid D3D scaling the framebuffer texture. This isn't a
  // problem with software compositing or ANGLE D3D with direct composition.
  const bool using_angle_d3d_compositing =
      gpu_compositing && !using_direct_composition;
  compositor->SetShouldDisableSwapUntilResize(using_angle_d3d_compositing);
#endif
}

gpu::ContextResult
VizProcessTransportFactory::TryCreateContextsForGpuCompositing(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host) {
  DCHECK(!is_gpu_compositing_disabled_);

  if (!gpu_channel_host && base::FeatureList::IsEnabled(
                               features::kShutdownForFailedChannelCreation)) {
    // If passed in `gpu_channel_host` is null, the previous EstablishGpuChannel
    // have failed. It means the gpu process have died but the browser UI thread
    // have not received child process disconnect signal. Manually remove it
    // before EstablishGpuChannel again. More in crbug.com/322909915.
    auto* gpu_process_host = GpuProcessHost::Get();
    if (gpu_process_host) {
      gpu_process_host->GpuProcessHost::ForceShutdown();
    }

    gpu_channel_host =
        gpu_channel_establish_factory_->EstablishGpuChannelSync();
    UMA_HISTOGRAM_BOOLEAN("GPU.EstablishGpuChannelSyncRetry",
                          !!gpu_channel_host);
  }

  if (!gpu_channel_host) {
    // Fallback to software compositing if there is no IPC channel.
    return gpu::ContextResult::kFatalFailure;
  }

  const auto& gpu_feature_info = gpu_channel_host->gpu_feature_info();
  // Fallback to software compositing if GPU compositing is blocklisted.
  // TODO(rivr): For now assume that if GL is blocklisted, then Vulkan is
  // also. Just check GL to see if GPU compositing is disabled.
  auto gpu_compositing_status =
      gpu_feature_info.status_values[gpu::GPU_FEATURE_TYPE_ACCELERATED_GL];
  if (gpu_compositing_status != gpu::kGpuFeatureStatusEnabled)
    return gpu::ContextResult::kFatalFailure;

  if (worker_context_provider_wrapper_ &&
      IsWorkerContextLost(
          worker_context_provider_wrapper_->GetContext().get())) {
    worker_context_provider_wrapper_.reset();
  }

  const bool enable_gpu_rasterization =
      features::IsUiGpuRasterizationEnabled() &&
      gpu_feature_info
              .status_values[gpu::GPU_FEATURE_TYPE_GPU_TILE_RASTERIZATION] ==
          gpu::kGpuFeatureStatusEnabled;

  if (!worker_context_provider_wrapper_) {
    // If the worker context supports GPU rasterization then UI tiles will be
    // rasterized on the GPU.
    auto worker_context_provider = CreateContextProvider(
        gpu_channel_host, /*supports_locking=*/true, enable_gpu_rasterization,
        viz::command_buffer_metrics::ContextType::BROWSER_WORKER);

    // Don't observer context loss on |worker_context_provider_wrapper_| here,
    // that is already observed by LayerTreeFrameSink. The lost context will
    // be caught when recreating LayerTreeFrameSink(s).
    auto context_result = worker_context_provider->BindToCurrentSequence();
    if (context_result != gpu::ContextResult::kSuccess)
      return context_result;

    worker_context_provider_wrapper_ =
        base::MakeRefCounted<cc::RasterContextProviderWrapper>(
            std::move(worker_context_provider), /*dark_mode_filter=*/nullptr,
            cc::ImageDecodeCacheUtils::GetWorkingSetBytesForImageDecode(
                /*for_renderer=*/false));
  }

  if (main_context_provider_ && IsContextLost(main_context_provider_.get())) {
    main_context_provider_.reset();
  }

  if (!main_context_provider_) {
    // The main thread context is not used for UI tile rasterization. Other UI
    // code can use the main thread context for GPU rasterization if it's
    // enabled for tiles.
    main_context_provider_ = CreateContextProvider(
        std::move(gpu_channel_host), /*supports_locking=*/false,
        enable_gpu_rasterization,
        viz::command_buffer_metrics::ContextType::BROWSER_MAIN_THREAD);
    main_context_provider_->SetDefaultTaskRunner(resize_task_runner_);

    auto context_result = main_context_provider_->BindToCurrentSequence();
    if (context_result != gpu::ContextResult::kSuccess) {
      main_context_provider_.reset();
      return context_result;
    }
  }

  return gpu::ContextResult::kSuccess;
}

VizProcessTransportFactory::CompositorData::CompositorData() = default;
VizProcessTransportFactory::CompositorData::CompositorData(
    CompositorData&& other) = default;
VizProcessTransportFactory::CompositorData&
VizProcessTransportFactory::CompositorData::operator=(CompositorData&& other) =
    default;
VizProcessTransportFactory::CompositorData::~CompositorData() = default;

}  // namespace content
