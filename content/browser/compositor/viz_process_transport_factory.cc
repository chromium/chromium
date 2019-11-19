// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/viz_process_transport_factory.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "cc/mojo_embedder/async_layer_tree_frame_sink.h"
#include "cc/raster/single_thread_task_graph_runner.h"
#include "components/viz/client/hit_test_data_provider_draw_quad.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/switches.h"
#include "components/viz/host/gpu_host_impl.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/service/display_embedder/compositing_mode_reporter_impl.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/gpu_stream_constants.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/reflector.h"

#if defined(OS_WIN)
#include "ui/gfx/win/rendering_window_manager.h"
#endif

namespace content {
namespace {

// The client id for the browser process. It must not conflict with any
// child process client id.
constexpr uint32_t kBrowserClientId = 0u;

scoped_refptr<viz::ContextProviderCommandBuffer> CreateContextProvider(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    bool support_locking,
    bool support_gles2_interface,
    bool support_raster_interface,
    bool support_grcontext,
    bool support_oop_rasterization,
    viz::command_buffer_metrics::ContextType type) {
  constexpr bool kAutomaticFlushes = false;

  gpu::ContextCreationAttribs attributes;
  attributes.alpha_size = -1;
  attributes.depth_size = 0;
  attributes.stencil_size = 0;
  attributes.samples = 0;
  attributes.sample_buffers = 0;
  attributes.bind_generates_resource = false;
  attributes.lose_context_when_out_of_memory = true;
  attributes.buffer_preserved = false;
  attributes.enable_gles2_interface = support_gles2_interface;
  attributes.enable_raster_interface = support_raster_interface;
  attributes.enable_oop_rasterization = support_oop_rasterization;

  gpu::SharedMemoryLimits memory_limits =
      gpu::SharedMemoryLimits::ForDisplayCompositor();

  GURL url("chrome://gpu/VizProcessTransportFactory::CreateContextProvider");
  return base::MakeRefCounted<viz::ContextProviderCommandBuffer>(
      std::move(gpu_channel_host), gpu_memory_buffer_manager,
      kGpuStreamIdDefault, kGpuStreamPriorityUI, gpu::kNullSurfaceHandle,
      std::move(url), kAutomaticFlushes, support_locking, support_grcontext,
      memory_limits, attributes, type);
}

bool IsContextLost(viz::ContextProvider* context_provider) {
  return context_provider->ContextGL()->GetGraphicsResetStatusKHR() !=
         GL_NO_ERROR;
}

bool IsWorkerContextLost(viz::RasterContextProvider* context_provider) {
  viz::RasterContextProvider::ScopedRasterContextLock lock(context_provider);
  return lock.RasterInterface()->GetGraphicsResetStatusKHR() != GL_NO_ERROR;
}

}  // namespace

VizProcessTransportFactory::VizProcessTransportFactory(
    gpu::GpuChannelEstablishFactory* gpu_channel_establish_factory,
    scoped_refptr<base::SingleThreadTaskRunner> resize_task_runner,
    viz::CompositingModeReporterImpl* compositing_mode_reporter)
    : gpu_channel_establish_factory_(gpu_channel_establish_factory),
      compositing_mode_reporter_(compositing_mode_reporter),
      task_graph_runner_(std::make_unique<cc::SingleThreadTaskGraphRunner>()),
      context_factory_private_(
          kBrowserClientId,
          BrowserMainLoop::GetInstance()->host_frame_sink_manager(),
          resize_task_runner) {
  DCHECK(gpu_channel_establish_factory_);
  task_graph_runner_->Start("CompositorTileWorker1",
                            base::SimpleThread::Options());
  context_factory_private_.GetHostFrameSinkManager()->SetConnectionLostCallback(
      base::BindRepeating(&VizProcessTransportFactory::OnGpuProcessLost,
                          weak_ptr_factory_.GetWeakPtr()));

  if (GpuDataManagerImpl::GetInstance()->IsGpuCompositingDisabled()) {
    DisableGpuCompositing(nullptr);
  }
}

VizProcessTransportFactory::~VizProcessTransportFactory() {
  if (main_context_provider_)
    main_context_provider_->RemoveObserver(this);

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
  context_factory_private_.GetHostFrameSinkManager()->BindAndSetManager(
      std::move(frame_sink_manager_client_receiver),
      context_factory_private_.resize_task_runner(),
      std::move(frame_sink_manager));

  if (GpuDataManagerImpl::GetInstance()->GpuProcessStartAllowed()) {
    // Hop to the IO thread, then send the other side of interface to viz
    // process.
    auto connect_on_io_thread =
        [](mojo::PendingReceiver<viz::mojom::FrameSinkManager> receiver,
           mojo::PendingRemote<viz::mojom::FrameSinkManagerClient> client) {
          // There should always be a GpuProcessHost instance, and GPU process,
          // for running the compositor thread. The exception is during shutdown
          // the GPU process won't be restarted and GpuProcessHost::Get() can
          // return null.
          auto* gpu_process_host = GpuProcessHost::Get();
          if (gpu_process_host) {
            gpu_process_host->gpu_host()->ConnectFrameSinkManager(
                std::move(receiver), std::move(client));
          }
        };
    base::PostTask(FROM_HERE, {BrowserThread::IO},
                   base::BindOnce(connect_on_io_thread,
                                  std::move(frame_sink_manager_receiver),
                                  std::move(frame_sink_manager_client)));
  } else {
    DCHECK(!viz_compositor_thread_);

    // GPU process access is disabled. Start a new thread to run the display
    // compositor in-process and connect HostFrameSinkManager to it.
    viz_compositor_thread_ =
        std::make_unique<viz::VizCompositorThreadRunnerImpl>();

    viz::mojom::FrameSinkManagerParamsPtr params =
        viz::mojom::FrameSinkManagerParams::New();
    params->restart_id = viz::BeginFrameSource::kNotRestartableId;
    base::Optional<uint32_t> activation_deadline_in_frames =
        switches::GetDeadlineToSynchronizeSurfaces();
    params->use_activation_deadline = activation_deadline_in_frames.has_value();
    params->activation_deadline_in_frames =
        activation_deadline_in_frames.value_or(0u);
    params->frame_sink_manager = std::move(frame_sink_manager_receiver);
    params->frame_sink_manager_client = std::move(frame_sink_manager_client);
    viz_compositor_thread_->CreateFrameSinkManager(std::move(params));
  }
}

void VizProcessTransportFactory::CreateLayerTreeFrameSink(
    base::WeakPtr<ui::Compositor> compositor) {
#if defined(OS_WIN)
  gfx::RenderingWindowManager::GetInstance()->UnregisterParent(
      compositor->widget());
#endif

  // Create the data map entry so that we can set properties like output secure
  // while we are waiting for the GpuChannel to be established.
  context_factory_private_.AddCompositor(compositor.get());

  if (IsGpuCompositingDisabled() || compositor->force_software_compositor()) {
    OnEstablishedGpuChannel(compositor, nullptr);
    return;
  }
  gpu_channel_establish_factory_->EstablishGpuChannel(
      base::BindOnce(&VizProcessTransportFactory::OnEstablishedGpuChannel,
                     weak_ptr_factory_.GetWeakPtr(), compositor));
}

scoped_refptr<viz::ContextProvider>
VizProcessTransportFactory::SharedMainThreadContextProvider() {
  if (IsGpuCompositingDisabled())
    return nullptr;

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

scoped_refptr<viz::RasterContextProvider>
VizProcessTransportFactory::SharedMainThreadRasterContextProvider() {
  SharedMainThreadContextProvider();
  DCHECK(!main_context_provider_ || main_context_provider_->RasterInterface());
  return main_context_provider_;
}

void VizProcessTransportFactory::RemoveCompositor(ui::Compositor* compositor) {
  context_factory_private_.UnconfigureCompositor(compositor);
}

gpu::GpuMemoryBufferManager*
VizProcessTransportFactory::GetGpuMemoryBufferManager() {
  return gpu_channel_establish_factory_->GetGpuMemoryBufferManager();
}

cc::TaskGraphRunner* VizProcessTransportFactory::GetTaskGraphRunner() {
  return task_graph_runner_.get();
}

void VizProcessTransportFactory::AddObserver(
    ui::ContextFactoryObserver* observer) {
  observer_list_.AddObserver(observer);
}

void VizProcessTransportFactory::RemoveObserver(
    ui::ContextFactoryObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

bool VizProcessTransportFactory::SyncTokensRequiredForDisplayCompositor() {
  // The display compositor is out-of-process, so must be using a different
  // context from the UI compositor, and requires synchronization between them.
  return true;
}

void VizProcessTransportFactory::DisableGpuCompositing() {
  if (!IsGpuCompositingDisabled())
    DisableGpuCompositing(nullptr);
}

ui::ContextFactory* VizProcessTransportFactory::GetContextFactory() {
  return this;
}

ui::ContextFactoryPrivate*
VizProcessTransportFactory::GetContextFactoryPrivate() {
  return &context_factory_private_;
}

void VizProcessTransportFactory::OnContextLost() {
  // PostTask to avoid destroying |main_context_provider_| while it's still
  // informing observers about the context loss.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&VizProcessTransportFactory::OnLostMainThreadSharedContext,
                     weak_ptr_factory_.GetWeakPtr()));
}

void VizProcessTransportFactory::DisableGpuCompositing(
    ui::Compositor* guilty_compositor) {
#if defined(OS_CHROMEOS)
  ALLOW_UNUSED_LOCAL(compositing_mode_reporter_);
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

  DLOG(ERROR) << "Switching to software compositing.";

  context_factory_private_.set_is_gpu_compositing_disabled(true);

  // Change the result of GpuDataManagerImpl::IsGpuCompositingDisabled() before
  // notifying anything.
  GpuDataManagerImpl::GetInstance()->SetGpuCompositingDisabled();

  compositing_mode_reporter_->SetUsingSoftwareCompositing();

  // Drop our reference on the gpu contexts for the compositors.
  worker_context_provider_.reset();
  if (main_context_provider_) {
    main_context_provider_->RemoveObserver(this);
    main_context_provider_.reset();
  }

  // Consumers of the shared main thread context aren't CompositingModeWatchers,
  // so inform them about the context loss due to switching to software
  // compositing.
  OnLostMainThreadSharedContext();

  // Reemove the FrameSink from every compositor that needs to fall back to
  // software compositing.
  for (auto* compositor : context_factory_private_.GetAllCompositors()) {
    // The |guilty_compositor| is in the process of setting up its FrameSink
    // so removing it from |compositor_data_map_| would be both pointless and
    // the cause of a crash.
    // Compositors with force_software_compositor() do not follow the global
    // compositing mode, so they do not need to changed.
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

bool VizProcessTransportFactory::IsGpuCompositingDisabled() {
  return context_factory_private_.is_gpu_compositing_disabled();
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
      !IsGpuCompositingDisabled() && !compositor->force_software_compositor();

  if (gpu_compositing) {
    auto context_result =
        TryCreateContextsForGpuCompositing(std::move(gpu_channel_host));
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

  scoped_refptr<viz::ContextProvider> compositor_context;
  scoped_refptr<viz::RasterContextProvider> worker_context;
  if (gpu_compositing) {
    // Only pass the contexts to the compositor if it will use gpu compositing.
    compositor_context = main_context_provider_;
    worker_context = worker_context_provider_;
  }
  context_factory_private_.ConfigureCompositor(
      compositor, std::move(compositor_context), std::move(worker_context));
}

gpu::ContextResult
VizProcessTransportFactory::TryCreateContextsForGpuCompositing(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host) {
  DCHECK(!IsGpuCompositingDisabled());

  // Fallback to software compositing if there is no IPC channel.
  if (!gpu_channel_host)
    return gpu::ContextResult::kFatalFailure;

  const auto& gpu_feature_info = gpu_channel_host->gpu_feature_info();
  // Fallback to software compositing if GPU compositing is blacklisted.
  auto gpu_compositing_status =
      gpu_feature_info.status_values[gpu::GPU_FEATURE_TYPE_GPU_COMPOSITING];
  if (gpu_compositing_status != gpu::kGpuFeatureStatusEnabled)
    return gpu::ContextResult::kFatalFailure;

  if (worker_context_provider_ &&
      IsWorkerContextLost(worker_context_provider_.get()))
    worker_context_provider_.reset();

  bool enable_oop_rasterization =
      features::IsUiGpuRasterizationEnabled() &&
      gpu_feature_info.status_values[gpu::GPU_FEATURE_TYPE_OOP_RASTERIZATION] ==
          gpu::kGpuFeatureStatusEnabled;
  bool enable_gpu_rasterization =
      features::IsUiGpuRasterizationEnabled() && !enable_oop_rasterization;

  if (!worker_context_provider_) {
    worker_context_provider_ = CreateContextProvider(
        gpu_channel_host, GetGpuMemoryBufferManager(),
        /*supports_locking=*/true,
        /*supports_gles2=*/enable_gpu_rasterization,
        /*supports_raster=*/true,
        /*supports_grcontext=*/enable_gpu_rasterization,
        /*supports_oopr=*/enable_oop_rasterization,
        viz::command_buffer_metrics::ContextType::BROWSER_WORKER);

    // Don't observer context loss on |worker_context_provider_| here, that is
    // already observered by LayerTreeFrameSink. The lost context will be caught
    // when recreating LayerTreeFrameSink(s).
    auto context_result = worker_context_provider_->BindToCurrentThread();
    if (context_result != gpu::ContextResult::kSuccess) {
      worker_context_provider_.reset();
      return context_result;
    }
  }

  if (main_context_provider_ && IsContextLost(main_context_provider_.get())) {
    main_context_provider_->RemoveObserver(this);
    main_context_provider_.reset();
  }

  if (!main_context_provider_) {
    constexpr bool kCompositorContextSupportsLocking = false;
    constexpr bool kCompositorContextSupportsGLES2 = true;
    constexpr bool kCompositorContextSupportsRaster = true;
    // GrContext is needed for HUD layer.
    constexpr bool kCompositorContextSupportsGrContext = true;
    constexpr bool kCompositorContextSupportsOOPR = false;

    main_context_provider_ = CreateContextProvider(
        std::move(gpu_channel_host), GetGpuMemoryBufferManager(),
        kCompositorContextSupportsLocking, kCompositorContextSupportsGLES2,
        kCompositorContextSupportsRaster, kCompositorContextSupportsGrContext,
        kCompositorContextSupportsOOPR,
        viz::command_buffer_metrics::ContextType::BROWSER_MAIN_THREAD);
    main_context_provider_->SetDefaultTaskRunner(
        context_factory_private_.resize_task_runner());

    auto context_result = main_context_provider_->BindToCurrentThread();
    if (context_result != gpu::ContextResult::kSuccess) {
      main_context_provider_.reset();
      return context_result;
    }

    main_context_provider_->AddObserver(this);
  }

  return gpu::ContextResult::kSuccess;
}

void VizProcessTransportFactory::OnLostMainThreadSharedContext() {
  // It's possible that |main_context_provider_| was already reset in
  // OnEstablishedGpuChannel(), so check if it's lost before resetting here.
  if (main_context_provider_ && IsContextLost(main_context_provider_.get())) {
    main_context_provider_->RemoveObserver(this);
    main_context_provider_.reset();
  }

  for (auto& observer : observer_list_)
    observer.OnLostSharedContext();
}

}  // namespace content
