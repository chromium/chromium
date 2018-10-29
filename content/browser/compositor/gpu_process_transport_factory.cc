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
#include "components/viz/service/display_embedder/compositor_overlay_candidate_validator.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
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
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/host/gpu_memory_buffer_support.h"
#include "gpu/vulkan/buildflags.h"
#include "services/service_manager/runner/common/client_util.h"
#include "services/ws/public/cpp/gpu/context_provider_command_buffer.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/host/external_begin_frame_controller_client_impl.h"
#include "ui/compositor/layer.h"
#include "ui/display/display_switches.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_switches.h"

#if defined(USE_AURA)
#include "content/public/common/service_manager_connection.h"
#include "ui/aura/env.h"
#include "ui/aura/window_tree_host.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "components/viz/service/display_embedder/compositor_overlay_candidate_validator_win.h"
#include "components/viz/service/display_embedder/output_device_backing.h"
#include "components/viz/service/display_embedder/software_output_device_win.h"
#include "ui/gfx/win/rendering_window_manager.h"
#elif defined(USE_OZONE)
#include "components/viz/service/display_embedder/compositor_overlay_candidate_validator_ozone.h"
#include "components/viz/service/display_embedder/software_output_device_ozone.h"
#include "ui/ozone/public/overlay_candidates_ozone.h"
#include "ui/ozone/public/overlay_manager_ozone.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/ozone_switches.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#include "ui/ozone/public/surface_ozone_canvas.h"
#elif defined(USE_X11)
#include "components/viz/service/display_embedder/software_output_device_x11.h"
#elif defined(OS_MACOSX)
#include "components/viz/service/display_embedder/compositor_overlay_candidate_validator_mac.h"
#include "components/viz/service/display_embedder/software_output_device_mac.h"
#include "content/browser/compositor/gpu_output_surface_mac.h"
#include "ui/base/cocoa/remote_layer_api.h"
#include "ui/base/ui_base_switches.h"
#elif defined(OS_ANDROID)
#include "components/viz/service/display_embedder/compositor_overlay_candidate_validator_android.h"
#endif
#if !defined(GPU_SURFACE_HANDLE_IS_ACCELERATED_WINDOW)
#include "gpu/ipc/common/gpu_surface_tracker.h"
#endif

#if BUILDFLAG(ENABLE_VULKAN)
#include "components/viz/common/gpu/vulkan_in_process_context_provider.h"
#include "content/browser/compositor/vulkan_browser_compositor_output_surface.h"
#include "gpu/vulkan/init/vulkan_factory.h"
#endif

using viz::ContextProvider;
using gpu::gles2::GLES2Interface;

namespace {

// The client_id used here should not conflict with the client_id generated
// from RenderWidgetHostImpl.
constexpr uint32_t kDefaultClientId = 0u;

#if defined(OS_MACOSX)
bool IsCALayersDisabledFromCommandLine() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kDisableMacOverlays);
}
#endif

bool CheckWorkerContextLost(viz::RasterContextProvider* context_provider) {
  if (!context_provider)
    return false;

  viz::RasterContextProvider::ScopedRasterContextLock lock(context_provider);
  return lock.RasterInterface()->GetGraphicsResetStatusKHR() != GL_NO_ERROR;
}

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
  std::unique_ptr<ui::ExternalBeginFrameControllerClientImpl>
      external_begin_frame_controller_client;
  ReflectorImpl* reflector = nullptr;
  std::unique_ptr<viz::Display> display;
  std::unique_ptr<viz::mojom::DisplayClient> display_client;
  bool output_is_secure = false;
};

GpuProcessTransportFactory::GpuProcessTransportFactory(
    gpu::GpuChannelEstablishFactory* gpu_channel_factory,
    viz::CompositingModeReporterImpl* compositing_mode_reporter,
    viz::ServerSharedBitmapManager* server_shared_bitmap_manager,
    scoped_refptr<base::SingleThreadTaskRunner> resize_task_runner)
    : frame_sink_id_allocator_(kDefaultClientId),
      renderer_settings_(viz::CreateRendererSettings()),
      resize_task_runner_(std::move(resize_task_runner)),
      task_graph_runner_(new cc::SingleThreadTaskGraphRunner),
      gpu_channel_factory_(gpu_channel_factory),
      compositing_mode_reporter_(compositing_mode_reporter),
      server_shared_bitmap_manager_(server_shared_bitmap_manager),
      callback_factory_(this) {
  DCHECK(gpu_channel_factory_);

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableFrameRateLimit))
    disable_frame_rate_limit_ = true;

  if (command_line->HasSwitch(switches::kRunAllCompositorStagesBeforeDraw))
    wait_for_all_pipeline_stages_before_draw_ = true;

  task_graph_runner_->Start("CompositorTileWorker1",
                            base::SimpleThread::Options());
#if defined(OS_WIN)
  software_backing_ = std::make_unique<viz::OutputDeviceBacking>();
#endif

  if (command_line->HasSwitch(switches::kDisableGpu) ||
      command_line->HasSwitch(switches::kDisableGpuCompositing)) {
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
    return base::WrapUnique(new viz::SoftwareOutputDevice);

#if defined(USE_AURA)
  if (features::IsMultiProcessMash()) {
    NOTREACHED();
    return nullptr;
  }
#endif

  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if defined(OS_WIN)
  return CreateSoftwareOutputDeviceWinBrowser(widget, software_backing_.get());
#elif defined(USE_OZONE)
  ui::SurfaceFactoryOzone* factory =
      ui::OzonePlatform::GetInstance()->GetSurfaceFactoryOzone();
  std::unique_ptr<ui::SurfaceOzoneCanvas> surface_ozone =
      factory->CreateCanvasForWidget(widget);
  CHECK(surface_ozone);
  return std::make_unique<viz::SoftwareOutputDeviceOzone>(
      std::move(surface_ozone));
#elif defined(USE_X11)
  return std::make_unique<viz::SoftwareOutputDeviceX11>(widget);
#elif defined(OS_MACOSX)
  return std::make_unique<viz::SoftwareOutputDeviceMac>(std::move(task_runner));
#else
  NOTREACHED();
  return std::unique_ptr<viz::SoftwareOutputDevice>();
#endif
}

std::unique_ptr<viz::CompositorOverlayCandidateValidator>
CreateOverlayCandidateValidator(
#if defined(OS_MACOSX)
    gfx::AcceleratedWidget widget,
    bool disable_overlay_ca_layers) {
#else
    gfx::AcceleratedWidget widget) {
#endif
  std::unique_ptr<viz::CompositorOverlayCandidateValidator> validator;
#if defined(USE_OZONE)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  std::string enable_overlay_flag =
      command_line->GetSwitchValueASCII(switches::kEnableHardwareOverlays);

  ui::OzonePlatform* ozone_platform = ui::OzonePlatform::GetInstance();
  DCHECK(ozone_platform);
  ui::OverlayManagerOzone* overlay_manager =
      ozone_platform->GetOverlayManager();
  if (!command_line->HasSwitch(switches::kEnableHardwareOverlays) &&
      overlay_manager->SupportsOverlays()) {
    enable_overlay_flag = "single-fullscreen,single-on-top,underlay";
  }
  if (!enable_overlay_flag.empty()) {
    std::unique_ptr<ui::OverlayCandidatesOzone> overlay_candidates =
        ozone_platform->GetOverlayManager()->CreateOverlayCandidates(widget);
    validator.reset(new viz::CompositorOverlayCandidateValidatorOzone(
        std::move(overlay_candidates), enable_overlay_flag));
  }
#elif defined(OS_MACOSX)
  // Overlays are only supported through the remote layer API.
  if (ui::RemoteLayerAPISupported()) {
    static bool overlays_disabled_at_command_line =
        IsCALayersDisabledFromCommandLine();
    const bool ca_layers_disabled =
        overlays_disabled_at_command_line || disable_overlay_ca_layers;
    validator.reset(
        new viz::CompositorOverlayCandidateValidatorMac(ca_layers_disabled));
  }
#elif defined(OS_ANDROID)
  validator.reset(new viz::CompositorOverlayCandidateValidatorAndroid());
#elif defined(OS_WIN)
  validator = std::make_unique<viz::CompositorOverlayCandidateValidatorWin>();
#endif

  return validator;
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

#if defined(OS_WIN)
  gfx::RenderingWindowManager::GetInstance()->UnregisterParent(
      compositor->widget());
#endif

#if BUILDFLAG(ENABLE_VULKAN)
  const bool use_vulkan = static_cast<bool>(SharedVulkanContextProvider());
#else
  const bool use_vulkan = false;
#endif
  const bool use_gpu_compositing =
      !compositor->force_software_compositor() && !is_gpu_compositing_disabled_;
  if (use_gpu_compositing && !use_vulkan) {
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

#if defined(OS_WIN)
  gfx::RenderingWindowManager::GetInstance()->RegisterParent(
      compositor->widget());
#endif

#if BUILDFLAG(ENABLE_VULKAN)
  scoped_refptr<viz::VulkanInProcessContextProvider> vulkan_context_provider =
      SharedVulkanContextProvider();
  bool use_vulkan = vulkan_context_provider != nullptr;
#else
  bool use_vulkan = false;
#endif
  scoped_refptr<ws::ContextProviderCommandBuffer> context_provider;

  if (!use_gpu_compositing || use_vulkan) {
    // If not using GL compositing, don't keep the old shared worker context.
    shared_worker_context_provider_ = nullptr;
  } else if (!gpu_channel_host) {
    // Failed to establish a channel, which is a fatal error, so stop trying to
    // use gpu compositing.
    use_gpu_compositing = false;
    shared_worker_context_provider_ = nullptr;
  } else {
    // Drop the old shared worker context if it was lost.
    if (CheckWorkerContextLost(shared_worker_context_provider_.get()))
      shared_worker_context_provider_ = nullptr;

    if (!shared_worker_context_provider_) {
      const bool need_alpha_channel = false;
      const bool support_locking = true;
      const bool support_gles2_interface =
          features::IsUiGpuRasterizationEnabled();
      const bool support_raster_interface = true;
      const bool support_grcontext = features::IsUiGpuRasterizationEnabled();
      shared_worker_context_provider_ = CreateContextCommon(
          gpu_channel_host, gpu::kNullSurfaceHandle, need_alpha_channel,
          false /* support_stencil */, support_locking, support_gles2_interface,
          support_raster_interface, support_grcontext,
          ws::command_buffer_metrics::ContextType::BROWSER_WORKER);
      auto result = shared_worker_context_provider_->BindToCurrentThread();
      if (result != gpu::ContextResult::kSuccess) {
        shared_worker_context_provider_ = nullptr;
        if (gpu::IsFatalOrSurfaceFailure(result))
          use_gpu_compositing = false;
      }
    }

    // The |context_provider| is used for both the browser compositor and the
    // display compositor. If we failed to make a worker context, just start
    // over and try again.
    if (shared_worker_context_provider_) {
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
          ws::command_buffer_metrics::ContextType::BROWSER_COMPOSITOR);
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
      use_vulkan || (context_provider && shared_worker_context_provider_);
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
      shared_worker_context_provider_ = nullptr;
    } else {
      gpu_channel_factory_->EstablishGpuChannel(base::BindOnce(
          &GpuProcessTransportFactory::EstablishedGpuChannel,
          callback_factory_.GetWeakPtr(), compositor, use_gpu_compositing));
      return;
    }
  }

  BrowserCompositorOutputSurface::UpdateVSyncParametersCallback vsync_callback =
      base::Bind(&ui::Compositor::SetDisplayVSyncParameters, compositor);
  std::unique_ptr<BrowserCompositorOutputSurface> display_output_surface;
#if BUILDFLAG(ENABLE_VULKAN)
  std::unique_ptr<VulkanBrowserCompositorOutputSurface> vulkan_surface;
  if (vulkan_context_provider) {
    vulkan_surface.reset(new VulkanBrowserCompositorOutputSurface(
        vulkan_context_provider, vsync_callback));
    if (!vulkan_surface->Initialize(compositor.get()->widget())) {
      vulkan_surface->Destroy();
      vulkan_surface.reset();
    } else {
      display_output_surface = std::move(vulkan_surface);
    }
  }
#endif

  if (!display_output_surface) {
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
                                         compositor->task_runner()),
              std::move(vsync_callback));
    } else {
      DCHECK(context_provider);
      const auto& capabilities = context_provider->ContextCapabilities();
      if (data->surface_handle == gpu::kNullSurfaceHandle) {
        display_output_surface =
            std::make_unique<OffscreenBrowserCompositorOutputSurface>(
                context_provider, std::move(vsync_callback),
                std::unique_ptr<viz::CompositorOverlayCandidateValidator>());
      } else if (capabilities.surfaceless) {
#if defined(OS_MACOSX)
        const auto& gpu_feature_info = context_provider->GetGpuFeatureInfo();
        bool disable_overlay_ca_layers = gpu_feature_info.IsWorkaroundEnabled(
            gpu::DISABLE_OVERLAY_CA_LAYERS);
        display_output_surface = std::make_unique<GpuOutputSurfaceMac>(
            context_provider, data->surface_handle, vsync_callback,
            CreateOverlayCandidateValidator(compositor->widget(),
                                            disable_overlay_ca_layers),
            GetGpuMemoryBufferManager());
#else
        DCHECK(capabilities.texture_format_bgra8888);
        auto gpu_output_surface =
            std::make_unique<GpuSurfacelessBrowserCompositorOutputSurface>(
                context_provider, data->surface_handle,
                std::move(vsync_callback),
                CreateOverlayCandidateValidator(compositor->widget()),
                GL_TEXTURE_2D, GL_BGRA_EXT,
                display::DisplaySnapshot::PrimaryFormat(),
                GetGpuMemoryBufferManager());
        display_output_surface = std::move(gpu_output_surface);
#endif
      } else {
        std::unique_ptr<viz::CompositorOverlayCandidateValidator> validator;
#if defined(OS_WIN)
        if (capabilities.dc_layers && capabilities.use_dc_overlays_for_video)
          validator = CreateOverlayCandidateValidator(compositor->widget());
#elif !defined(OS_MACOSX)
        // Overlays are only supported on surfaceless output surfaces on Mac.
        validator = CreateOverlayCandidateValidator(compositor->widget());
#endif
        auto gpu_output_surface =
            std::make_unique<GpuBrowserCompositorOutputSurface>(
                context_provider, std::move(vsync_callback),
                std::move(validator));
        display_output_surface = std::move(gpu_output_surface);
      }
    }
  }

  data->display_output_surface = display_output_surface.get();
  if (data->reflector)
    data->reflector->OnSourceSurfaceReady(data->display_output_surface);

  std::unique_ptr<viz::SyntheticBeginFrameSource> synthetic_begin_frame_source;
  std::unique_ptr<viz::ExternalBeginFrameSourceMojo>
      external_begin_frame_source_mojo;
  std::unique_ptr<ui::ExternalBeginFrameControllerClientImpl>
      external_begin_frame_controller_client;

  viz::BeginFrameSource* begin_frame_source = nullptr;
  if (compositor->external_begin_frames_enabled()) {
    external_begin_frame_controller_client =
        std::make_unique<ui::ExternalBeginFrameControllerClientImpl>(
            compositor.get());
    // We don't bind the controller mojo interface, since we only use the
    // ExternalBeginFrameSourceMojo directly and not via mojo (plus, as it
    // is an associated interface, binding it would require a separate pipe).
    viz::mojom::ExternalBeginFrameControllerAssociatedRequest request = nullptr;
    external_begin_frame_source_mojo =
        std::make_unique<viz::ExternalBeginFrameSourceMojo>(
            std::move(request),
            external_begin_frame_controller_client->GetBoundPtr(),
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
      server_shared_bitmap_manager_, renderer_settings_,
      compositor->frame_sink_id(), std::move(display_output_surface),
      std::move(scheduler), compositor->task_runner());
  data->display_client =
      std::make_unique<viz::HostDisplayClient>(compositor->widget());
  GetFrameSinkManager()->RegisterBeginFrameSource(begin_frame_source,
                                                  compositor->frame_sink_id());
  // Note that we are careful not to destroy prior BeginFrameSource objects
  // until we have reset |data->display|.
  data->synthetic_begin_frame_source = std::move(synthetic_begin_frame_source);
  data->external_begin_frame_source_mojo =
      std::move(external_begin_frame_source_mojo);
  data->external_begin_frame_controller_client =
      std::move(external_begin_frame_controller_client);
  if (data->external_begin_frame_source_mojo)
    data->external_begin_frame_source_mojo->SetDisplay(data->display.get());

  // The |delegated_output_surface| is given back to the compositor, it
  // delegates to the Display as its root surface. Importantly, it shares the
  // same ContextProvider as the Display's output surface.
  auto layer_tree_frame_sink = std::make_unique<viz::DirectLayerTreeFrameSink>(
      compositor->frame_sink_id(), GetHostFrameSinkManager(),
      GetFrameSinkManager(), data->display.get(), data->display_client.get(),
      context_provider, shared_worker_context_provider_,
      compositor->task_runner(), GetGpuMemoryBufferManager(),
      features::IsVizHitTestingEnabled());
  data->display->Resize(compositor->size());
  data->display->SetOutputIsSecure(data->output_is_secure);
  compositor->SetLayerTreeFrameSink(std::move(layer_tree_frame_sink));
}

void GpuProcessTransportFactory::DisableGpuCompositing(
    ui::Compositor* guilty_compositor) {
  DLOG(ERROR) << "Switching to software compositing.";

  // Change the result of IsGpuCompositingDisabled() before notifying anything.
  is_gpu_compositing_disabled_ = true;

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

  GpuDataManagerImpl::GetInstance()->NotifyGpuInfoUpdate();
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
  if (auto* source_surface = source_data->display_output_surface)
    reflector->OnSourceSurfaceReady(source_surface);
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
#if defined(OS_WIN)
  gfx::RenderingWindowManager::GetInstance()->UnregisterParent(
      compositor->widget());
#endif
}

double GpuProcessTransportFactory::GetRefreshRate() const {
  return 60.0;
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

bool GpuProcessTransportFactory::IsGpuCompositingDisabled() {
  return is_gpu_compositing_disabled_;
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
    const gfx::ColorSpace& blending_color_space,
    const gfx::ColorSpace& output_color_space) {
  auto it = per_compositor_data_.find(compositor);
  if (it == per_compositor_data_.end())
    return;
  PerCompositorData* data = it->second.get();
  DCHECK(data);
  // The compositor will always SetColorSpace on the Display once it is set up,
  // so do nothing if |display| is null.
  if (data->display)
    data->display->SetColorSpace(blending_color_space, output_color_space);
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
  }
}

void GpuProcessTransportFactory::IssueExternalBeginFrame(
    ui::Compositor* compositor,
    const viz::BeginFrameArgs& args) {
  auto it = per_compositor_data_.find(compositor);
  if (it == per_compositor_data_.end())
    return;
  PerCompositorData* data = it->second.get();
  DCHECK(data);
  DCHECK(data->external_begin_frame_source_mojo);
  data->external_begin_frame_source_mojo->IssueExternalBeginFrame(args);
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

viz::FrameSinkManagerImpl* GpuProcessTransportFactory::GetFrameSinkManager() {
  return BrowserMainLoop::GetInstance()->GetFrameSinkManager();
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
  bool support_raster_interface = false;
  bool support_grcontext = true;
  shared_main_thread_contexts_ = CreateContextCommon(
      std::move(gpu_channel_host), gpu::kNullSurfaceHandle, need_alpha_channel,
      false, support_locking, support_gles2_interface, support_raster_interface,
      support_grcontext,
      ws::command_buffer_metrics::ContextType::BROWSER_MAIN_THREAD);
  shared_main_thread_contexts_->AddObserver(this);
  auto result = shared_main_thread_contexts_->BindToCurrentThread();
  if (result != gpu::ContextResult::kSuccess) {
    shared_main_thread_contexts_->RemoveObserver(this);
    shared_main_thread_contexts_ = nullptr;
  }
  return shared_main_thread_contexts_;
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
  LOG(ERROR) << "Lost UI shared context.";

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

#if BUILDFLAG(ENABLE_VULKAN)
scoped_refptr<viz::VulkanInProcessContextProvider>
GpuProcessTransportFactory::SharedVulkanContextProvider() {
  if (!shared_vulkan_context_provider_initialized_) {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableVulkan)) {
      base::ScopedAllowBlocking allow_blocking;
      vulkan_implementation_ = gpu::CreateVulkanImplementation();
      if (vulkan_implementation_ &&
          vulkan_implementation_->InitializeVulkanInstance()) {
        shared_vulkan_context_provider_ =
            viz::VulkanInProcessContextProvider::Create(
                vulkan_implementation_.get());
      } else {
        vulkan_implementation_.reset();
      }
    }

    shared_vulkan_context_provider_initialized_ = true;
  }
  return shared_vulkan_context_provider_;
}
#endif

void GpuProcessTransportFactory::OnContextLost() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuProcessTransportFactory::OnLostMainThreadSharedContext,
                     callback_factory_.GetWeakPtr()));
}

scoped_refptr<ws::ContextProviderCommandBuffer>
GpuProcessTransportFactory::CreateContextCommon(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
    gpu::SurfaceHandle surface_handle,
    bool need_alpha_channel,
    bool need_stencil_bits,
    bool support_locking,
    bool support_gles2_interface,
    bool support_raster_interface,
    bool support_grcontext,
    ws::command_buffer_metrics::ContextType type) {
  DCHECK(gpu_channel_host);
  DCHECK(!is_gpu_compositing_disabled_);

  // All browser contexts get the same stream id and priority.
  int32_t stream_id = content::kGpuStreamIdDefault;
  gpu::SchedulingPriority stream_priority = content::kGpuStreamPriorityUI;

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

  GURL url("chrome://gpu/GpuProcessTransportFactory::CreateContextCommon");
  return base::MakeRefCounted<ws::ContextProviderCommandBuffer>(
      std::move(gpu_channel_host), GetGpuMemoryBufferManager(), stream_id,
      stream_priority, surface_handle, url, automatic_flushes, support_locking,
      support_grcontext, memory_limits, attributes, type);
}

}  // namespace content
