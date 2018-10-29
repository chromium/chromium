// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/viz_process_transport_factory.h"

#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "cc/mojo_embedder/async_layer_tree_frame_sink.h"
#include "cc/raster/single_thread_task_graph_runner.h"
#include "components/viz/client/hit_test_data_provider_draw_quad.h"
#include "components/viz/client/local_surface_id_provider.h"
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
#include "services/ws/public/cpp/gpu/context_provider_command_buffer.h"
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

scoped_refptr<ws::ContextProviderCommandBuffer> CreateContextProviderImpl(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    bool support_locking,
    bool support_gles2_interface,
    bool support_raster_interface,
    bool support_grcontext,
    bool support_oop_rasterization,
    ws::command_buffer_metrics::ContextType type) {
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
  return base::MakeRefCounted<ws::ContextProviderCommandBuffer>(
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

// Provided as a callback to crash the GPU process.
void ReceivedBadMessageFromGpuProcess() {
  GpuProcessHost::CallOnIO(
      GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED, false /* force_create */,
      base::BindRepeating([](GpuProcessHost* host) {
        // There should always be a GpuProcessHost instance, and GPU process,
        // for running the compositor thread. The exception is during shutdown
        // the GPU process won't be restarted and GpuProcessHost::Get() can
        // return null.
        if (host)
          host->ForceShutdown();

        LOG(ERROR) << "Bad message received, terminating gpu process.";
        base::debug::DumpWithoutCrashing();
      }));
}

}  // namespace

VizProcessTransportFactory::VizProcessTransportFactory(
    gpu::GpuChannelEstablishFactory* gpu_channel_establish_factory,
    scoped_refptr<base::SingleThreadTaskRunner> resize_task_runner,
    viz::CompositingModeReporterImpl* compositing_mode_reporter)
    : ui::HostContextFactoryPrivate(
          kBrowserClientId,
          BrowserMainLoop::GetInstance()->host_frame_sink_manager(),
          resize_task_runner),
      gpu_channel_establish_factory_(gpu_channel_establish_factory),
      compositing_mode_reporter_(compositing_mode_reporter),
      task_graph_runner_(std::make_unique<cc::SingleThreadTaskGraphRunner>()),
      weak_ptr_factory_(this) {
  DCHECK(gpu_channel_establish_factory_);
  task_graph_runner_->Start("CompositorTileWorker1",
                            base::SimpleThread::Options());
  GetHostFrameSinkManager()->SetConnectionLostCallback(
      base::BindRepeating(&VizProcessTransportFactory::OnGpuProcessLost,
                          weak_ptr_factory_.GetWeakPtr()));
  GetHostFrameSinkManager()->SetBadMessageReceivedFromGpuCallback(
      base::BindRepeating(&ReceivedBadMessageFromGpuProcess));

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableGpu) ||
      command_line->HasSwitch(switches::kDisableGpuCompositing)) {
    DisableGpuCompositing(nullptr);
  }
}

VizProcessTransportFactory::~VizProcessTransportFactory() {
  if (main_context_provider_)
    main_context_provider_->RemoveObserver(this);

  task_graph_runner_->Shutdown();
}

void VizProcessTransportFactory::ConnectHostFrameSinkManager() {
  viz::mojom::FrameSinkManagerPtr frame_sink_manager;
  viz::mojom::FrameSinkManagerRequest frame_sink_manager_request =
      mojo::MakeRequest(&frame_sink_manager);
  viz::mojom::FrameSinkManagerClientPtr frame_sink_manager_client;
  viz::mojom::FrameSinkManagerClientRequest frame_sink_manager_client_request =
      mojo::MakeRequest(&frame_sink_manager_client);

  // Setup HostFrameSinkManager with interface endpoints.
  GetHostFrameSinkManager()->BindAndSetManager(
      std::move(frame_sink_manager_client_request), resize_task_runner(),
      std::move(frame_sink_manager));

  if (GpuDataManagerImpl::GetInstance()->GpuProcessStartAllowed()) {
    // Hop to the IO thread, then send the other side of interface to viz
    // process.
    auto connect_on_io_thread =
        [](viz::mojom::FrameSinkManagerRequest request,
           viz::mojom::FrameSinkManagerClientPtrInfo client) {
          // There should always be a GpuProcessHost instance, and GPU process,
          // for running the compositor thread. The exception is during shutdown
          // the GPU process won't be restarted and GpuProcessHost::Get() can
          // return null.
          auto* gpu_process_host = GpuProcessHost::Get();
          if (gpu_process_host) {
            gpu_process_host->gpu_host()->ConnectFrameSinkManager(
                std::move(request), std::move(client));
          }
        };
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(connect_on_io_thread,
                       std::move(frame_sink_manager_request),
                       frame_sink_manager_client.PassInterface()));
  } else {
    DCHECK(!viz_compositor_thread_);

    // GPU process access is disabled. Start a new thread to run the display
    // compositor in-process and connect HostFrameSinkManager to it.
    viz_compositor_thread_ = std::make_unique<viz::VizCompositorThreadRunner>();

    viz::mojom::FrameSinkManagerParamsPtr params =
        viz::mojom::FrameSinkManagerParams::New();
    params->restart_id = viz::BeginFrameSource::kNotRestartableId;
    base::Optional<uint32_t> activation_deadline_in_frames =
        switches::GetDeadlineToSynchronizeSurfaces();
    params->use_activation_deadline = activation_deadline_in_frames.has_value();
    params->activation_deadline_in_frames =
        activation_deadline_in_frames.value_or(0u);
    params->frame_sink_manager = std::move(frame_sink_manager_request);
    params->frame_sink_manager_client =
        frame_sink_manager_client.PassInterface();
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
  AddCompositor(compositor.get());

  if (is_gpu_compositing_disabled() ||
      compositor->force_software_compositor()) {
    OnEstablishedGpuChannel(compositor, nullptr);
    return;
  }
  gpu_channel_establish_factory_->EstablishGpuChannel(
      base::BindOnce(&VizProcessTransportFactory::OnEstablishedGpuChannel,
                     weak_ptr_factory_.GetWeakPtr(), compositor));
}

scoped_refptr<viz::ContextProvider>
VizProcessTransportFactory::SharedMainThreadContextProvider() {
  if (is_gpu_compositing_disabled())
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

void VizProcessTransportFactory::RemoveCompositor(ui::Compositor* compositor) {
  UnconfigureCompositor(compositor);
}

double VizProcessTransportFactory::GetRefreshRate() const {
  // TODO(kylechar): Delete this function from ContextFactoryPrivate.
  return 60.0;
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
  if (!is_gpu_compositing_disabled())
    DisableGpuCompositing(nullptr);
}

bool VizProcessTransportFactory::IsGpuCompositingDisabled() {
  return is_gpu_compositing_disabled();
}

ui::ContextFactory* VizProcessTransportFactory::GetContextFactory() {
  return this;
}

ui::ContextFactoryPrivate*
VizProcessTransportFactory::GetContextFactoryPrivate() {
  return this;
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
  DLOG(ERROR) << "Switching to software compositing.";

  // Change the result of IsGpuCompositingDisabled() before notifying anything.
  set_is_gpu_compositing_disabled(true);

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
  for (ui::Compositor* compositor : GetAllCompositors()) {
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

  GpuDataManagerImpl::GetInstance()->NotifyGpuInfoUpdate();
}

void VizProcessTransportFactory::OnGpuProcessLost() {
  // Reconnect HostFrameSinkManager to new GPU process.
  ConnectHostFrameSinkManager();

  for (auto& observer : observer_list_)
    observer.OnLostVizProcess();
}

void VizProcessTransportFactory::OnEstablishedGpuChannel(
    base::WeakPtr<ui::Compositor> compositor_weak_ptr,
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host) {
  ui::Compositor* compositor = compositor_weak_ptr.get();
  if (!compositor)
    return;

  bool gpu_compositing = !is_gpu_compositing_disabled() &&
                         !compositor->force_software_compositor();

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
  ConfigureCompositor(compositor, std::move(compositor_context),
                      std::move(worker_context));
}

gpu::ContextResult
VizProcessTransportFactory::TryCreateContextsForGpuCompositing(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host) {
  DCHECK(!is_gpu_compositing_disabled());

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
      gpu_feature_info.status_values[gpu::GPU_FEATURE_TYPE_OOP_RASTERIZATION] ==
      gpu::kGpuFeatureStatusEnabled;

  if (!worker_context_provider_) {
    constexpr bool kSharedWorkerContextSupportsLocking = true;
    constexpr bool kSharedWorkerContextSupportsRaster = true;
    const bool kSharedWorkerContextSupportsGLES2 =
        features::IsUiGpuRasterizationEnabled() && !enable_oop_rasterization;
    const bool kSharedWorkerContextSupportsGrContext =
        features::IsUiGpuRasterizationEnabled() && !enable_oop_rasterization;
    const bool kSharedWorkerContextSupportsOOPR = enable_oop_rasterization;

    worker_context_provider_ = CreateContextProviderImpl(
        gpu_channel_host, GetGpuMemoryBufferManager(),
        kSharedWorkerContextSupportsLocking, kSharedWorkerContextSupportsGLES2,
        kSharedWorkerContextSupportsRaster,
        kSharedWorkerContextSupportsGrContext, kSharedWorkerContextSupportsOOPR,
        ws::command_buffer_metrics::ContextType::BROWSER_WORKER);

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
    constexpr bool kCompositorContextSupportsRaster = false;
    constexpr bool kCompositorContextSupportsGrContext = true;
    constexpr bool kCompositorContextSupportsOOPR = false;

    main_context_provider_ = CreateContextProviderImpl(
        std::move(gpu_channel_host), GetGpuMemoryBufferManager(),
        kCompositorContextSupportsLocking, kCompositorContextSupportsGLES2,
        kCompositorContextSupportsRaster, kCompositorContextSupportsGrContext,
        kCompositorContextSupportsOOPR,
        ws::command_buffer_metrics::ContextType::BROWSER_MAIN_THREAD);
    main_context_provider_->SetDefaultTaskRunner(resize_task_runner());

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
