// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/gpu_process_transport_factory.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "cc/base/histograms.h"
#include "cc/raster/single_thread_task_graph_runner.h"
#include "cc/raster/task_graph_runner.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/frame_sinks/delay_based_time_source.h"
#include "components/viz/common/switches.h"
#include "components/viz/host/host_display_client.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/host/renderer_settings_creation.h"
#include "components/viz/service/display/display.h"
#include "components/viz/service/display/display_scheduler.h"
#include "components/viz/service/display_embedder/compositing_mode_reporter_impl.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/display_embedder/vsync_parameter_listener.h"
#include "components/viz/service/frame_sinks/direct_layer_tree_frame_sink.h"
#include "components/viz/service/frame_sinks/external_begin_frame_source_mojo.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/compositor/browser_compositor_output_surface.h"
#include "content/browser/compositor/gpu_browser_compositor_output_surface.h"
#include "content/browser/compositor/gpu_surfaceless_browser_compositor_output_surface.h"
#include "content/browser/compositor/offscreen_browser_compositor_output_surface.h"
#include "content/browser/compositor/reflector_impl.h"
#include "content/browser/compositor/software_browser_compositor_output_surface.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/gpu_stream_constants.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/host/gpu_memory_buffer_support.h"
#include "gpu/vulkan/buildflags.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/display/display_switches.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_switches.h"

#if defined(USE_OZONE)
#include "components/viz/service/display_embedder/software_output_device_ozone.h"
#include "ui/ozone/public/overlay_candidates_ozone.h"
#include "ui/ozone/public/overlay_manager_ozone.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/ozone_switches.h"
#include "ui/ozone/public/platform_window_surface.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#include "ui/ozone/public/surface_ozone_canvas.h"
#elif defined(USE_X11)
#include "components/viz/service/display_embedder/software_output_device_x11.h"
#endif
#if !defined(GPU_SURFACE_HANDLE_IS_ACCELERATED_WINDOW)
#include "gpu/ipc/common/gpu_surface_tracker.h"
#endif

using viz::ContextProvider;
using gpu::gles2::GLES2Interface;

namespace {

// The client_id used here should not conflict with the client_id generated
// from RenderWidgetHostImpl.
constexpr uint32_t kDefaultClientId = 0u;

// Id used in creating ContextProviderCommandBuffer.
constexpr int32_t kStreamId = content::kGpuStreamIdDefault;

// Url identity supplied to ContextProviderCommandBuffer.
constexpr char kIdentityUrl[] =
    "chrome://gpu/GpuProcessTransportFactory::CreateContextCommon";

// All browser contexts get the same stream id and priority.
constexpr gpu::SchedulingPriority kStreamPriority =
    content::kGpuStreamPriorityUI;

viz::FrameSinkManagerImpl* GetFrameSinkManager() {
  return content::BrowserMainLoop::GetInstance()->GetFrameSinkManager();
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
class HostDisplayClient : public viz::HostDisplayClient {
 public:
  explicit HostDisplayClient(ui::Compositor* compositor)
      : viz::HostDisplayClient(compositor->widget()), compositor_(compositor) {}
  ~HostDisplayClient() override = default;

  // viz::HostDisplayClient:
  void DidCompleteSwapWithNewSize(const gfx::Size& size) override {
    compositor_->OnCompleteSwapWithNewSize(size);
  }

 private:
  ui::Compositor* const compositor_;

  DISALLOW_COPY_AND_ASSIGN(HostDisplayClient);
};
#else
class HostDisplayClient : public viz::HostDisplayClient {
 public:
  explicit HostDisplayClient(ui::Compositor* compositor)
      : viz::HostDisplayClient(compositor->widget()) {}
  ~HostDisplayClient() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(HostDisplayClient);
};
#endif

}  // namespace

namespace content {

struct GpuProcessTransportFactory::PerCompositorData {
  gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  BrowserCompositorOutputSurface* display_output_surface = nullptr;
  // Exactly one of |synthetic_begin_frame_source| and
  // |external_begin_frame_source| is valid at the same time.
  std::unique_ptr<viz::SyntheticBeginFrameSource> synthetic_begin_frame_source;
  std::unique_ptr<viz::ExternalBeginFrameSourceMojo>
      external_begin_frame_source_mojo;
  ReflectorImpl* reflector = nullptr;
  std::unique_ptr<viz::Display> display;
  std::unique_ptr<viz::mojom::DisplayClient> display_client;
  bool output_is_secure = false;
  std::unique_ptr<viz::VSyncParameterListener> vsync_listener;
};

GpuProcessTransportFactory::GpuProcessTransportFactory(
    gpu::GpuChannelEstablishFactory* gpu_channel_factory,
    viz::CompositingModeReporterImpl* compositing_mode_reporter,
    viz::ServerSharedBitmapManager* server_shared_bitmap_manager,
    scoped_refptr<base::SingleThreadTaskRunner> resize_task_runner)
    : frame_sink_id_allocator_(kDefaultClientId),
      resize_task_runner_(std::move(resize_task_runner)),
      task_graph_runner_(new cc::SingleThreadTaskGraphRunner),
      shared_worker_context_provider_factory_(
          kStreamId,
          kStreamPriority,
          GURL(kIdentityUrl),
          viz::command_buffer_metrics::ContextType::BROWSER_WORKER),
      gpu_channel_factory_(gpu_channel_factory),
      compositing_mode_reporter_(compositing_mode_reporter),
      server_shared_bitmap_manager_(server_shared_bitmap_manager) {
  DCHECK(gpu_channel_factory_);

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableFrameRateLimit))
    disable_frame_rate_limit_ = true;

  if (command_line->HasSwitch(switches::kRunAllCompositorStagesBeforeDraw))
    wait_for_all_pipeline_stages_before_draw_ = true;

  task_graph_runner_->Start("CompositorTileWorker1",
                            base::SimpleThread::Options());

  if (GpuDataManagerImpl::GetInstance()->IsGpuCompositingDisabled()) {
    DisableGpuCompositing(nullptr);
  }
}

GpuProcessTransportFactory::~GpuProcessTransportFactory() {
  DCHECK(per_compositor_data_.empty());

  if (shared_main_thread_contexts_)
    shared_main_thread_contexts_->RemoveObserver(this);

  // Make sure the lost context callback doesn't try to run during destruction.
  callback_factory_.InvalidateWeakPtrs();

  task_graph_runner_->Shutdown();
}

std::unique_ptr<viz::SoftwareOutputDevice>
GpuProcessTransportFactory::CreateSoftwareOutputDevice(
    gfx::AcceleratedWidget widget,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kHeadless))
    return std::make_unique<viz::SoftwareOutputDevice>();

  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if defined(USE_OZONE)
  ui::SurfaceFactoryOzone* factory =
      ui::OzonePlatform::GetInstance()->GetSurfaceFactoryOzone();
  std::unique_ptr<ui::PlatformWindowSurface> platform_window_surface =
      factory->CreatePlatformWindowSurface(widget);
  std::unique_ptr<ui::SurfaceOzoneCanvas> surface_ozone =
      factory->CreateCanvasForWidget(widget, task_runner.get());
  CHECK(surface_ozone);
  return std::make_unique<viz::SoftwareOutputDeviceOzone>(
      std::move(platform_window_surface), std::move(surface_ozone));
#elif defined(USE_X11)
  return std::make_unique<viz::SoftwareOutputDeviceX11>(widget,
                                                        task_runner.get());
#else
  NOTREACHED();
  return nullptr;
#endif
}

void GpuProcessTransportFactory::CreateLayerTreeFrameSink(
    base::WeakPtr<ui::Compositor> compositor) {
  DCHECK(!!compositor);
  PerCompositorData* data = per_compositor_data_[compositor.get()].get();
  if (!data) {
    data = CreatePerCompositorData(compositor.get());
  } else {
    // TODO(danakj): We can destroy the |data->display| and
    // |data->begin_frame_source| here when the compositor destroys its
    // LayerTreeFrameSink before calling back here.
    data->display_output_surface = nullptr;
  }

  const bool use_gpu_compositing =
      !compositor->force_software_compositor() && !is_gpu_compositing_disabled_;
  if (use_gpu_compositing) {
    gpu_channel_factory_->EstablishGpuChannel(base::BindOnce(
        &GpuProcessTransportFactory::EstablishedGpuChannel,
        callback_factory_.GetWeakPtr(), compositor, use_gpu_compositing));
  } else {
    EstablishedGpuChannel(compositor, use_gpu_compositing, nullptr);
  }
}

void GpuProcessTransportFactory::EstablishedGpuChannel(
    base::WeakPtr<ui::Compositor> compositor,
    bool use_gpu_compositing,
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host) {
  if (!compositor)
    return;

  if (gpu_channel_host &&
      gpu_channel_host->gpu_feature_info()
              .status_values[gpu::GPU_FEATURE_TYPE_GPU_COMPOSITING] !=
          gpu::kGpuFeatureStatusEnabled) {
    use_gpu_compositing = false;
  }
  // Gpu compositing may have been disabled in the meantime.
  if (is_gpu_compositing_disabled_)
    use_gpu_compositing = false;

  // The widget might have been released in the meantime.
  auto it = per_compositor_data_.find(compositor.get());
  if (it == per_compositor_data_.end())
    return;

  PerCompositorData* data = it->second.get();
  DCHECK(data);

  bool support_stencil = false;
#if defined(OS_CHROMEOS)
  // ChromeOS uses surfaceless when running on a real device and stencil
  // buffers can then be added dynamically so supporting them does not have an
  // impact on normal usage. If we are not running on a real ChromeOS device
  // but instead on a workstation for development, then stencil support is
  // useful as it allows the overdraw feedback debugging feature to be used.
  support_stencil = true;
#endif

  scoped_refptr<viz::ContextProviderCommandBuffer> context_provider;

  if (!use_gpu_compositing) {
    // If not using GL compositing, don't keep the old shared worker context.
    shared_worker_context_provider_factory_.Reset();
  } else if (!gpu_channel_host) {
    // Failed to establish a channel, which is a fatal error, so stop trying to
    // use gpu compositing.
    use_gpu_compositing = false;
    shared_worker_context_provider_factory_.Reset();
  } else {
    auto shared_worker_validate_result =
        shared_worker_context_provider_factory_.Validate(
            gpu_channel_host, GetGpuMemoryBufferManager());
    if (shared_worker_validate_result != gpu::ContextResult::kSuccess) {
      shared_worker_context_provider_factory_.Reset();
      if (gpu::IsFatalOrSurfaceFailure(shared_worker_validate_result))
        use_gpu_compositing = false;
    }

    // The |context_provider| is used for both the browser compositor and the
    // display compositor. If we failed to make a worker context, just start
    // over and try again.
    if (shared_worker_context_provider()) {
      // For mus, we create an offscreen context for a mus window, and we will
      // use CommandBufferProxyImpl::TakeFrontBuffer() to take the context's
      // front buffer into a mailbox, insert a sync token, and send the
      // mailbox+sync to the ui service process.
      gpu::SurfaceHandle surface_handle = data->surface_handle;
      const bool need_alpha_channel = false;
      const bool support_locking = false;
      const bool support_gles2_interface = true;
      const bool support_raster_interface = false;
      const bool support_grcontext = true;
      context_provider = CreateContextCommon(
          std::move(gpu_channel_host), surface_handle, need_alpha_channel,
          support_stencil, support_locking, support_gles2_interface,
          support_raster_interface, support_grcontext,
          viz::command_buffer_metrics::ContextType::BROWSER_COMPOSITOR);
      // On Mac, GpuCommandBufferMsg_SwapBuffersCompleted must be handled in
      // a nested run loop during resize.
      context_provider->SetDefaultTaskRunner(resize_task_runner_);
      auto result = context_provider->BindToCurrentThread();
      if (result != gpu::ContextResult::kSuccess) {
        context_provider = nullptr;
        if (gpu::IsFatalOrSurfaceFailure(result))
          use_gpu_compositing = false;
      }
    }
  }

  bool gpu_compositing_ready =
      context_provider && shared_worker_context_provider();
  UMA_HISTOGRAM_BOOLEAN("Aura.CreatedGpuBrowserCompositor",
                        gpu_compositing_ready);
  if (!gpu_compositing_ready) {
#if defined(OS_CHROMEOS)
    // A fatal context error occured, and we can not fall back to software
    // compositing on ChromeOS. These can be unrecoverable hardware errors,
    // or bugs that should not happen: either from the client's context request,
    // in the service, or a transient error was miscategorized as fatal.
    CHECK(use_gpu_compositing);
#endif

    // Try again if we didn't give up on gpu. Otherwise, drop the shared context
    // if it exists and won't be used.
    if (!use_gpu_compositing) {
      shared_worker_context_provider_factory_.Reset();
    } else {
      gpu_channel_factory_->EstablishGpuChannel(base::BindOnce(
          &GpuProcessTransportFactory::EstablishedGpuChannel,
          callback_factory_.GetWeakPtr(), compositor, use_gpu_compositing));
      return;
    }
  }

  std::unique_ptr<BrowserCompositorOutputSurface> display_output_surface;
  if (!use_gpu_compositing) {
    if (!is_gpu_compositing_disabled_ &&
        !compositor->force_software_compositor()) {
      // This will cause all other display compositors and FrameSink clients
      // to fall back to software compositing. If the compositor is
      // |force_software_compositor()|, then it is not a signal to others to
      // use software too - but such compositors can not embed external
      // surfaces as they are not following the correct mode.
      DisableGpuCompositing(compositor.get());
    }
    display_output_surface =
        std::make_unique<SoftwareBrowserCompositorOutputSurface>(
            CreateSoftwareOutputDevice(compositor->widget(),
                                       compositor->task_runner()));
  } else {
    DCHECK(context_provider);
    const auto& capabilities = context_provider->ContextCapabilities();
    if (data->surface_handle == gpu::kNullSurfaceHandle) {
      display_output_surface =
          std::make_unique<OffscreenBrowserCompositorOutputSurface>(
              context_provider);
    } else if (capabilities.surfaceless) {
      DCHECK(capabilities.texture_format_bgra8888);
      auto gpu_output_surface =
          std::make_unique<GpuSurfacelessBrowserCompositorOutputSurface>(
              context_provider, data->surface_handle,
              display::DisplaySnapshot::PrimaryFormat(),
              GetGpuMemoryBufferManager());
      display_output_surface = std::move(gpu_output_surface);
    } else {
      auto gpu_output_surface =
          std::make_unique<GpuBrowserCompositorOutputSurface>(
              context_provider, data->surface_handle);
      display_output_surface = std::move(gpu_output_surface);
    }
  }

  auto vsync_callback = base::BindRepeating(
      &ui::Compositor::SetDisplayVSyncParameters, compositor);
  display_output_surface->SetUpdateVSyncParametersCallback(vsync_callback);

  data->display_output_surface = display_output_surface.get();
  if (data->reflector) {
    data->reflector->OnSourceSurfaceReady(data->display_output_surface);
  }

  std::unique_ptr<viz::SyntheticBeginFrameSource> synthetic_begin_frame_source;
  std::unique_ptr<viz::ExternalBeginFrameSourceMojo>
      external_begin_frame_source_mojo;
  viz::BeginFrameSource* begin_frame_source = nullptr;
  if (compositor->use_external_begin_frame_control()) {
    // We don't bind the controller mojo interface, since we only use the
    // ExternalBeginFrameSourceMojo directly and not via mojo (plus, as it
    // is an associated remote, binding it would require a separate pipe).
    external_begin_frame_source_mojo =
        std::make_unique<viz::ExternalBeginFrameSourceMojo>(
            GetFrameSinkManager(), mojo::NullAssociatedReceiver(),
            viz::BeginFrameSource::kNotRestartableId);
    begin_frame_source = external_begin_frame_source_mojo.get();
  } else if (disable_frame_rate_limit_) {
    synthetic_begin_frame_source =
        std::make_unique<viz::BackToBackBeginFrameSource>(
            std::make_unique<viz::DelayBasedTimeSource>(
                compositor->task_runner().get()));
    begin_frame_source = synthetic_begin_frame_source.get();
  } else {
    synthetic_begin_frame_source =
        std::make_unique<viz::DelayBasedBeginFrameSource>(
            std::make_unique<viz::DelayBasedTimeSource>(
                compositor->task_runner().get()),
            viz::BeginFrameSource::kNotRestartableId);
    begin_frame_source = synthetic_begin_frame_source.get();
  }

  if (data->synthetic_begin_frame_source) {
    GetFrameSinkManager()->UnregisterBeginFrameSource(
        data->synthetic_begin_frame_source.get());
  } else if (data->external_begin_frame_source_mojo) {
    GetFrameSinkManager()->UnregisterBeginFrameSource(
        data->external_begin_frame_source_mojo.get());
    data->external_begin_frame_source_mojo->SetDisplay(nullptr);
  }

  auto scheduler = std::make_unique<viz::DisplayScheduler>(
      begin_frame_source, compositor->task_runner().get(),
      display_output_surface->capabilities().max_frames_pending,
      wait_for_all_pipeline_stages_before_draw_);

  // The Display owns and uses the |display_output_surface| created above.
  data->display = std::make_unique<viz::Display>(
      server_shared_bitmap_manager_, viz::CreateRendererSettings(),
      compositor->frame_sink_id(), std::move(display_output_surface),
      std::move(scheduler), compositor->task_runner());
  data->display_client = std::make_unique<HostDisplayClient>(compositor.get());
  GetFrameSinkManager()->RegisterBeginFrameSource(begin_frame_source,
                                                  compositor->frame_sink_id());
  // Note that we are careful not to destroy prior BeginFrameSource objects
  // until we have reset |data->display|.
  data->synthetic_begin_frame_source = std::move(synthetic_begin_frame_source);
  data->external_begin_frame_source_mojo =
      std::move(external_begin_frame_source_mojo);
  if (data->external_begin_frame_source_mojo)
    data->external_begin_frame_source_mojo->SetDisplay(data->display.get());

  // The |delegated_output_surface| is given back to the compositor, it
  // delegates to the Display as its root surface. Importantly, it shares the
  // same ContextProvider as the Display's output surface.
  auto layer_tree_frame_sink = std::make_unique<viz::DirectLayerTreeFrameSink>(
      compositor->frame_sink_id(), GetHostFrameSinkManager(),
      GetFrameSinkManager(), data->display.get(), data->display_client.get(),
      context_provider, shared_worker_context_provider(),
      compositor->task_runner(), GetGpuMemoryBufferManager(),
      features::IsVizHitTestingSurfaceLayerEnabled());
  data->display->Resize(compositor->size());
  data->display->SetOutputIsSecure(data->output_is_secure);
  data->display->SetDisplayTransformHint(compositor->display_transform());
  compositor->SetLayerTreeFrameSink(std::move(layer_tree_frame_sink));
}

void GpuProcessTransportFactory::DisableGpuCompositing(
    ui::Compositor* guilty_compositor) {
  DLOG(ERROR) << "Switching to software compositing.";

  is_gpu_compositing_disabled_ = true;

  // Change the result of GpuDataManagerImpl::IsGpuCompositingDisabled() before
  // notifying anything.
  GpuDataManagerImpl::GetInstance()->SetGpuCompositingDisabled();

  // This will notify all CompositingModeWatchers.
  compositing_mode_reporter_->SetUsingSoftwareCompositing();

  // Consumers of the shared main thread context aren't CompositingModeWatchers,
  // so inform them about the compositing mode switch by acting like the context
  // was lost. This also destroys the contexts since they aren't created when
  // gpu compositing isn't being used.
  OnLostMainThreadSharedContext();

  // This class chooses the compositing mode for all ui::Compositors and display
  // compositors, so it is not a CompositingModeWatcher also. Here we remove the
  // FrameSink from every compositor that needs to fall back to software
  // compositing (except the |guilty_compositor| which is already doing so).
  //
  // Releasing the FrameSink from the compositor will remove it from
  // |per_compositor_data_|, so we can't do that while iterating though the
  // collection.
  std::vector<ui::Compositor*> to_release;
  to_release.reserve(per_compositor_data_.size());
  for (auto& pair : per_compositor_data_) {
    ui::Compositor* compositor = pair.first;
    // The |guilty_compositor| is in the process of setting up its FrameSink
    // so removing it from |per_compositor_data_| would be both pointless and
    // the cause of a crash.
    // Compositors with |force_software_compositor()| do not follow the global
    // compositing mode, so they do not need to changed.
    if (compositor != guilty_compositor &&
        !compositor->force_software_compositor())
      to_release.push_back(compositor);
  }
  for (ui::Compositor* compositor : to_release) {
    // Compositor expects to be not visible when releasing its FrameSink.
    bool visible = compositor->IsVisible();
    compositor->SetVisible(false);
    gfx::AcceleratedWidget widget = compositor->ReleaseAcceleratedWidget();
    compositor->SetAcceleratedWidget(widget);
    if (visible)
      compositor->SetVisible(true);
  }
}

std::unique_ptr<ui::Reflector> GpuProcessTransportFactory::CreateReflector(
    ui::Compositor* source_compositor,
    ui::Layer* target_layer) {
  PerCompositorData* source_data =
      per_compositor_data_[source_compositor].get();
  DCHECK(source_data);

  std::unique_ptr<ReflectorImpl> reflector(
      new ReflectorImpl(source_compositor, target_layer));
  source_data->reflector = reflector.get();
  if (auto* source_surface = source_data->display_output_surface) {
    reflector->OnSourceSurfaceReady(source_surface);
  }
  return std::move(reflector);
}

void GpuProcessTransportFactory::RemoveReflector(ui::Reflector* reflector) {
  ReflectorImpl* reflector_impl = static_cast<ReflectorImpl*>(reflector);
  PerCompositorData* data =
      per_compositor_data_[reflector_impl->mirrored_compositor()].get();
  DCHECK(data);
  data->reflector->Shutdown();
  data->reflector = nullptr;
}

void GpuProcessTransportFactory::RemoveCompositor(ui::Compositor* compositor) {
  auto it = per_compositor_data_.find(compositor);
  if (it == per_compositor_data_.end())
    return;
  PerCompositorData* data = it->second.get();
  DCHECK(data);
#if !defined(GPU_SURFACE_HANDLE_IS_ACCELERATED_WINDOW)
  if (data->surface_handle)
    gpu::GpuSurfaceTracker::Get()->RemoveSurface(data->surface_handle);
#endif
  if (data->synthetic_begin_frame_source) {
    GetFrameSinkManager()->UnregisterBeginFrameSource(
        data->synthetic_begin_frame_source.get());
  } else if (data->external_begin_frame_source_mojo) {
    GetFrameSinkManager()->UnregisterBeginFrameSource(
        data->external_begin_frame_source_mojo.get());
    data->external_begin_frame_source_mojo->SetDisplay(nullptr);
  }
  per_compositor_data_.erase(it);
  if (per_compositor_data_.empty()) {
    // If there are any observers left at this point, notify them that the
    // context has been lost.
    for (auto& observer : observer_list_)
      observer.OnLostSharedContext();
  }
}

gpu::GpuMemoryBufferManager*
GpuProcessTransportFactory::GetGpuMemoryBufferManager() {
  return gpu_channel_factory_->GetGpuMemoryBufferManager();
}

cc::TaskGraphRunner* GpuProcessTransportFactory::GetTaskGraphRunner() {
  return task_graph_runner_.get();
}

void GpuProcessTransportFactory::DisableGpuCompositing() {
  if (!is_gpu_compositing_disabled_)
    DisableGpuCompositing(nullptr);
}

ui::ContextFactory* GpuProcessTransportFactory::GetContextFactory() {
  return this;
}

ui::ContextFactoryPrivate*
GpuProcessTransportFactory::GetContextFactoryPrivate() {
  return this;
}

viz::FrameSinkId GpuProcessTransportFactory::AllocateFrameSinkId() {
  return frame_sink_id_allocator_.NextFrameSinkId();
}

viz::HostFrameSinkManager*
GpuProcessTransportFactory::GetHostFrameSinkManager() {
  return BrowserMainLoop::GetInstance()->host_frame_sink_manager();
}

void GpuProcessTransportFactory::SetDisplayVisible(ui::Compositor* compositor,
                                                   bool visible) {
  auto it = per_compositor_data_.find(compositor);
  if (it == per_compositor_data_.end())
    return;
  PerCompositorData* data = it->second.get();
  DCHECK(data);
  // The compositor will always SetVisible on the Display once it is set up, so
  // do nothing if |display| is null.
  if (data->display)
    data->display->SetVisible(visible);
}

void GpuProcessTransportFactory::ResizeDisplay(ui::Compositor* compositor,
                                               const gfx::Size& size) {
  auto it = per_compositor_data_.find(compositor);
  if (it == per_compositor_data_.end())
    return;
  PerCompositorData* data = it->second.get();
  DCHECK(data);
  if (data->display)
    data->display->Resize(size);
}

void GpuProcessTransportFactory::DisableSwapUntilResize(
    ui::Compositor* compositor) {
  auto it = per_compositor_data_.find(compositor);
  if (it == per_compositor_data_.end())
    return;
  PerCompositorData* data = it->second.get();
  DCHECK(data);
  if (data->display)
    data->display->Resize(gfx::Size());
}

void GpuProcessTransportFactory::SetDisplayColorMatrix(
    ui::Compositor* compositor,
    const SkMatrix44& matrix) {
  auto it = per_compositor_data_.find(compositor);
  if (it == per_compositor_data_.end())
    return;
  PerCompositorData* data = it->second.get();
  DCHECK(data);

  if (data->display)
    data->display->SetColorMatrix(matrix);
}

void GpuProcessTransportFactory::SetDisplayColorSpace(
    ui::Compositor* compositor,
    const gfx::ColorSpace& output_color_space,
    float sdr_white_level) {
  auto it = per_compositor_data_.find(compositor);
  if (it == per_compositor_data_.end())
    return;
  PerCompositorData* data = it->second.get();
  DCHECK(data);
  // The compositor will always SetColorSpace on the Display once it is set up,
  // so do nothing if |display| is null.
  if (data->display)
    data->display->SetColorSpace(output_color_space, sdr_white_level);
}

void GpuProcessTransportFactory::SetDisplayVSyncParameters(
    ui::Compositor* compositor,
    base::TimeTicks timebase,
    base::TimeDelta interval) {
  auto it = per_compositor_data_.find(compositor);
  if (it == per_compositor_data_.end())
    return;
  PerCompositorData* data = it->second.get();
  DCHECK(data);
  if (data->synthetic_begin_frame_source) {
    data->synthetic_begin_frame_source->OnUpdateVSyncParameters(timebase,
                                                                interval);
    if (data->vsync_listener)
      data->vsync_listener->OnVSyncParametersUpdated(timebase, interval);
  }
}

void GpuProcessTransportFactory::IssueExternalBeginFrame(
    ui::Compositor* compositor,
    const viz::BeginFrameArgs& args,
    bool force,
    base::OnceCallback<void(const viz::BeginFrameAck&)> callback) {
  auto it = per_compositor_data_.find(compositor);
  if (it == per_compositor_data_.end())
    return;
  PerCompositorData* data = it->second.get();
  DCHECK(data);
  DCHECK(data->external_begin_frame_source_mojo);
  data->external_begin_frame_source_mojo->IssueExternalBeginFrame(
      args, force, std::move(callback));
}

void GpuProcessTransportFactory::SetOutputIsSecure(ui::Compositor* compositor,
                                                   bool secure) {
  auto it = per_compositor_data_.find(compositor);
  if (it == per_compositor_data_.end())
    return;
  PerCompositorData* data = it->second.get();
  DCHECK(data);
  data->output_is_secure = secure;
  if (data->display)
    data->display->SetOutputIsSecure(secure);
}

void GpuProcessTransportFactory::AddVSyncParameterObserver(
    ui::Compositor* compositor,
    mojo::PendingRemote<viz::mojom::VSyncParameterObserver> observer) {
  auto it = per_compositor_data_.find(compositor);
  if (it == per_compositor_data_.end())
    return;
  PerCompositorData* data = it->second.get();
  DCHECK(data);
  data->vsync_listener =
      std::make_unique<viz::VSyncParameterListener>(std::move(observer));
}

void GpuProcessTransportFactory::SetDisplayTransformHint(
    ui::Compositor* compositor,
    gfx::OverlayTransform transform) {
  auto it = per_compositor_data_.find(compositor);
  if (it == per_compositor_data_.end())
    return;
  PerCompositorData* data = it->second.get();
  DCHECK(data);
  if (data->display)
    data->display->SetDisplayTransformHint(transform);
}

void GpuProcessTransportFactory::AddObserver(
    ui::ContextFactoryObserver* observer) {
  observer_list_.AddObserver(observer);
}

void GpuProcessTransportFactory::RemoveObserver(
    ui::ContextFactoryObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

bool GpuProcessTransportFactory::SyncTokensRequiredForDisplayCompositor() {
  // Display and DirectLayerTreeFrameSink share a GL context, so sync
  // points aren't needed when passing resources between them.
  return false;
}

scoped_refptr<ContextProvider>
GpuProcessTransportFactory::SharedMainThreadContextProvider() {
  if (is_gpu_compositing_disabled_)
    return nullptr;

  if (shared_main_thread_contexts_)
    return shared_main_thread_contexts_;

  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host =
      gpu_channel_factory_->EstablishGpuChannelSync();
  if (!gpu_channel_host ||
      gpu_channel_host->gpu_feature_info()
              .status_values[gpu::GPU_FEATURE_TYPE_GPU_COMPOSITING] !=
          gpu::kGpuFeatureStatusEnabled) {
    DisableGpuCompositing(nullptr);
    if (gpu_channel_host)
      gpu_channel_host->DestroyChannel();
    return nullptr;
  }

  bool need_alpha_channel = false;
  bool support_locking = false;
  bool support_gles2_interface = true;
  bool support_raster_interface = true;
  bool support_grcontext = false;
  shared_main_thread_contexts_ = CreateContextCommon(
      std::move(gpu_channel_host), gpu::kNullSurfaceHandle, need_alpha_channel,
      false, support_locking, support_gles2_interface, support_raster_interface,
      support_grcontext,
      viz::command_buffer_metrics::ContextType::BROWSER_MAIN_THREAD);
  shared_main_thread_contexts_->AddObserver(this);
  auto result = shared_main_thread_contexts_->BindToCurrentThread();
  if (result != gpu::ContextResult::kSuccess) {
    shared_main_thread_contexts_->RemoveObserver(this);
    shared_main_thread_contexts_ = nullptr;
  }
  return shared_main_thread_contexts_;
}

scoped_refptr<viz::RasterContextProvider>
GpuProcessTransportFactory::SharedMainThreadRasterContextProvider() {
  SharedMainThreadContextProvider();
  DCHECK(!shared_main_thread_contexts_ ||
         shared_main_thread_contexts_->RasterInterface());
  return shared_main_thread_contexts_;
}

scoped_refptr<viz::RasterContextProvider>
GpuProcessTransportFactory::shared_worker_context_provider() {
  return shared_worker_context_provider_factory_.provider();
}

GpuProcessTransportFactory::PerCompositorData*
GpuProcessTransportFactory::CreatePerCompositorData(
    ui::Compositor* compositor) {
  DCHECK(!per_compositor_data_[compositor]);

  gfx::AcceleratedWidget widget = compositor->widget();

  auto data = std::make_unique<PerCompositorData>();
  if (widget == gfx::kNullAcceleratedWidget) {
    data->surface_handle = gpu::kNullSurfaceHandle;
  } else {
#if defined(GPU_SURFACE_HANDLE_IS_ACCELERATED_WINDOW)
    data->surface_handle = widget;
#else
    gpu::GpuSurfaceTracker* tracker = gpu::GpuSurfaceTracker::Get();
    data->surface_handle = tracker->AddSurfaceForNativeWidget(
        gpu::GpuSurfaceTracker::SurfaceRecord(widget));
#endif
  }

  PerCompositorData* return_ptr = data.get();
  per_compositor_data_[compositor] = std::move(data);
  return return_ptr;
}

void GpuProcessTransportFactory::OnLostMainThreadSharedContext() {
  // Keep old resources around while we call the observers, but ensure that
  // new resources are created if needed.
  // Kill shared contexts for both threads in tandem so they are always in
  // the same share group.
  if (shared_main_thread_contexts_)
    shared_main_thread_contexts_->RemoveObserver(this);
  scoped_refptr<ContextProvider> lost_shared_main_thread_contexts =
      shared_main_thread_contexts_;
  shared_main_thread_contexts_ = nullptr;

  for (auto& observer : observer_list_)
    observer.OnLostSharedContext();

  // Kill things that use the shared context before killing the shared context.
  lost_shared_main_thread_contexts = nullptr;
}

void GpuProcessTransportFactory::OnContextLost() {
  DLOG(ERROR) << "Lost UI shared context.";

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuProcessTransportFactory::OnLostMainThreadSharedContext,
                     callback_factory_.GetWeakPtr()));
}

scoped_refptr<viz::ContextProviderCommandBuffer>
GpuProcessTransportFactory::CreateContextCommon(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
    gpu::SurfaceHandle surface_handle,
    bool need_alpha_channel,
    bool need_stencil_bits,
    bool support_locking,
    bool support_gles2_interface,
    bool support_raster_interface,
    bool support_grcontext,
    viz::command_buffer_metrics::ContextType type) {
  DCHECK(gpu_channel_host);
  DCHECK(!is_gpu_compositing_disabled_);

  // This is called from a few places to create different contexts:
  // - The shared main thread context (offscreen).
  // - The compositor context, which is used by the browser compositor
  //   (offscreen) for synchronization mostly, and by the display compositor
  //   (onscreen, except for with mus) for actual GL drawing.
  // - The compositor worker context (offscreen) used for GPU raster.
  // So ask for capabilities needed by any of these cases (we can optimize by
  // branching on |surface_handle| being null if these needs diverge).
  //
  // The default framebuffer for an offscreen context is not used, so it does
  // not need alpha, stencil, depth, antialiasing. The display compositor does
  // not use these things either (except for alpha when using mus for
  // non-opaque ui that overlaps the system's window borders or stencil bits
  // for overdraw feedback), so we can request only that when needed.
  gpu::ContextCreationAttribs attributes;
  attributes.alpha_size = need_alpha_channel ? 8 : -1;
  attributes.depth_size = 0;
  attributes.stencil_size = need_stencil_bits ? 8 : 0;
  attributes.samples = 0;
  attributes.sample_buffers = 0;
  attributes.bind_generates_resource = false;
  attributes.lose_context_when_out_of_memory = true;
  attributes.buffer_preserved = false;
  attributes.enable_gles2_interface = support_gles2_interface;
  attributes.enable_raster_interface = support_raster_interface;

  gpu::SharedMemoryLimits memory_limits =
      gpu::SharedMemoryLimits::ForDisplayCompositor();

  constexpr bool automatic_flushes = false;

  return base::MakeRefCounted<viz::ContextProviderCommandBuffer>(
      std::move(gpu_channel_host), GetGpuMemoryBufferManager(), kStreamId,
      kStreamPriority, surface_handle, GURL(kIdentityUrl), automatic_flushes,
      support_locking, support_grcontext, memory_limits, attributes, type);
}

}  // namespace content
