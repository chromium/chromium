// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/compositor_impl_android.h"

#include <android/bitmap.h>
#include <android/native_window_jni.h>
#include <stdint.h>

#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/android/application_status_listener.h"
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/hash_tables.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/sys_info.h"
#include "base/task/post_task.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/animation/animation_host.h"
#include "cc/base/switches.h"
#include "cc/input/input_handler.h"
#include "cc/layers/layer.h"
#include "cc/mojo_embedder/async_layer_tree_frame_sink.h"
#include "cc/raster/single_thread_task_graph_runner.h"
#include "cc/resources/ui_resource_manager.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_settings.h"
#include "components/viz/client/frame_eviction_manager.h"
#include "components/viz/client/hit_test_data_provider_draw_quad.h"
#include "components/viz/client/local_surface_id_provider.h"
#include "components/viz/common/features.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/gpu/vulkan_in_process_context_provider.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/surfaces/frame_sink_id_allocator.h"
#include "components/viz/host/host_display_client.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/service/display/display.h"
#include "components/viz/service/display/display_scheduler.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display_embedder/compositor_overlay_candidate_validator_android.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/direct_layer_tree_frame_sink.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/android/compositor_client.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/gpu_stream_constants.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/command_buffer/common/swap_buffers_flags.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/gpu_surface_tracker.h"
#include "gpu/vulkan/buildflags.h"
#if BUILDFLAG(ENABLE_VULKAN)
#include "gpu/vulkan/init/vulkan_factory.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_surface.h"
#endif
#include "services/viz/privileged/interfaces/compositing/frame_sink_manager.mojom.h"
#include "services/viz/public/interfaces/compositing/compositor_frame_sink.mojom.h"
#include "services/ws/public/cpp/gpu/context_provider_command_buffer.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkMallocPixelRef.h"
#include "ui/android/window_android.h"
#include "ui/compositor/host/external_begin_frame_controller_client_impl.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/ca_layer_params.h"
#include "ui/gfx/swap_result.h"
#include "ui/gl/gl_utils.h"
#include "ui/latency/latency_tracker.h"

namespace gpu {
class VulkanSurface;
}

namespace content {

namespace {

static const char* kBrowser = "Browser";

// These functions are called based on application visibility status.
void SendOnBackgroundedToGpuService() {
  content::GpuProcessHost::CallOnIO(
      content::GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED,
      false /* force_create */,
      base::BindRepeating([](content::GpuProcessHost* host) {
        if (host) {
          host->gpu_service()->OnBackgrounded();
        }
      }));
}

void SendOnForegroundedToGpuService() {
  content::GpuProcessHost::CallOnIO(
      content::GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED,
      false /* force_create */,
      base::BindRepeating([](content::GpuProcessHost* host) {
        if (host) {
          host->gpu_service()->OnForegrounded();
        }
      }));
}

void BrowserGpuChannelHostFactorySetApplicationVisible(bool is_visible) {
  // This code relies on the browser's GpuChannelEstablishFactory being the
  // BrowserGpuChannelHostFactory.
  DCHECK_EQ(BrowserMainLoop::GetInstance()->gpu_channel_establish_factory(),
            BrowserGpuChannelHostFactory::instance());
  BrowserGpuChannelHostFactory::instance()->SetApplicationVisible(is_visible);
}

// The client_id used here should not conflict with the client_id generated
// from RenderWidgetHostImpl.
constexpr uint32_t kDefaultClientId = 0u;

class SingleThreadTaskGraphRunner : public cc::SingleThreadTaskGraphRunner {
 public:
  SingleThreadTaskGraphRunner() {
    Start("CompositorTileWorker1", base::SimpleThread::Options());
  }

  ~SingleThreadTaskGraphRunner() override { Shutdown(); }
};

// An implementation of HostDisplayClient which handles swap callbacks.
class AndroidHostDisplayClient : public viz::HostDisplayClient {
 public:
  explicit AndroidHostDisplayClient(
      base::RepeatingCallback<void(const gfx::Size&)> on_swap,
      base::RepeatingCallback<void(gpu::ContextResult)>
          on_context_creation_failure)
      : HostDisplayClient(gfx::kNullAcceleratedWidget),
        on_swap_(std::move(on_swap)),
        on_context_creation_failure_(std::move(on_context_creation_failure)) {}

  // viz::mojom::DisplayClient implementation:
  void DidCompleteSwapWithSize(const gfx::Size& pixel_size) override {
    if (on_swap_)
      on_swap_.Run(pixel_size);
  }
  void OnFatalOrSurfaceContextCreationFailure(
      gpu::ContextResult context_result) override {
    if (on_context_creation_failure_)
      on_context_creation_failure_.Run(context_result);
  }

 private:
  base::RepeatingCallback<void(const gfx::Size&)> on_swap_;
  base::RepeatingCallback<void(gpu::ContextResult)>
      on_context_creation_failure_;
};

class CompositorDependencies {
 public:
  static CompositorDependencies& Get() {
    static base::NoDestructor<CompositorDependencies> instance;
    return *instance;
  }

  void CreateVizFrameSinkManager() {
    viz::mojom::FrameSinkManagerPtr frame_sink_manager;
    viz::mojom::FrameSinkManagerRequest frame_sink_manager_request =
        mojo::MakeRequest(&frame_sink_manager);
    viz::mojom::FrameSinkManagerClientPtr frame_sink_manager_client;
    viz::mojom::FrameSinkManagerClientRequest
        frame_sink_manager_client_request =
            mojo::MakeRequest(&frame_sink_manager_client);

    // Setup HostFrameSinkManager with interface endpoints.
    host_frame_sink_manager.BindAndSetManager(
        std::move(frame_sink_manager_client_request),
        base::ThreadTaskRunnerHandle::Get(), std::move(frame_sink_manager));

    // Set up a callback to automatically re-connect if we lose our
    // connection.
    host_frame_sink_manager.SetConnectionLostCallback(base::BindRepeating(
        []() { CompositorDependencies::Get().CreateVizFrameSinkManager(); }));

    pending_connect_viz_on_main_thread_ = base::BindOnce(
        &CompositorDependencies::
            OnReadyToConnectVizFrameSinkManagerOnMainThread,
        base::Unretained(this), std::move(frame_sink_manager_request),
        frame_sink_manager_client.PassInterface());

    // Will connect using the above callback if we are foreground.
    TryEstablishVizConnectionIfNeeded();
  }

  SingleThreadTaskGraphRunner task_graph_runner;
  viz::HostFrameSinkManager host_frame_sink_manager;
  viz::FrameSinkIdAllocator frame_sink_id_allocator;
  viz::ParentLocalSurfaceIdAllocator surface_id_allocator;

  // Non-viz members:
  // This is owned here so that SurfaceManager will be accessible in process
  // when display is in the same process. Other than using SurfaceManager,
  // access to |in_process_frame_sink_manager_| should happen via
  // |host_frame_sink_manager_| instead which uses Mojo. See
  // http://crbug.com/657959.
  std::unique_ptr<viz::FrameSinkManagerImpl> frame_sink_manager_impl;

#if BUILDFLAG(ENABLE_VULKAN)
  std::unique_ptr<gpu::VulkanImplementation> vulkan_implementation;
  scoped_refptr<viz::VulkanContextProvider> vulkan_context_provider;
#endif
 private:
  friend class base::NoDestructor<CompositorDependencies>;

  CompositorDependencies()
      : frame_sink_id_allocator(kDefaultClientId),
        app_listener_(
            base::android::ApplicationStatusListener::New(base::BindRepeating(
                &CompositorDependencies::OnApplicationStateChange,
                base::Unretained(this)))) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    // Ensure we're in the correct state at start up.
    OnApplicationStateChange(app_listener_->GetState());

    bool enable_viz =
        base::FeatureList::IsEnabled(features::kVizDisplayCompositor);
    if (!enable_viz) {
      // The SharedBitmapManager can be null as software compositing is not
      // supported or used on Android.
      frame_sink_manager_impl = std::make_unique<viz::FrameSinkManagerImpl>(
          /*shared_bitmap_manager=*/nullptr);
      surface_utils::ConnectWithLocalFrameSinkManager(
          &host_frame_sink_manager, frame_sink_manager_impl.get());
    } else {
      CreateVizFrameSinkManager();
    }
  }

  void OnReadyToConnectVizFrameSinkManagerOnMainThread(
      viz::mojom::FrameSinkManagerRequest request,
      viz::mojom::FrameSinkManagerClientPtrInfo client,
      scoped_refptr<gpu::GpuChannelHost> host) {
    if (!host) {
      // If host creation failed, try again. We have no software fallback on
      // Android. This must succeed.
      CreateVizFrameSinkManager();
      return;
    }

    // Forward |connect_on_io| to the IO thread to run.
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&CompositorDependencies::
                           OnReadyToConnectVizFrameSinkManagerOnIOThread,
                       base::Unretained(this), std::move(request),
                       std::move(client)));
  }

  void OnReadyToConnectVizFrameSinkManagerOnIOThread(
      viz::mojom::FrameSinkManagerRequest request,
      viz::mojom::FrameSinkManagerClientPtrInfo client) {
    // There should always be a GpuProcessHost instance, and GPU
    // process at this point. The exception is
    // during shutdown the GPU process won't be restarted and
    // GpuProcessHost::Get() can return null.
    auto* gpu_process_host = GpuProcessHost::Get();
    if (gpu_process_host) {
      gpu_process_host->gpu_host()->ConnectFrameSinkManager(std::move(request),
                                                            std::move(client));
    }
  }

  void TryEstablishVizConnectionIfNeeded() {
    // We don't connect to the viz process if backgrounded, as the OS may
    // repeatedly kill the resulting process. Instead wait until we come to the
    // foreground.
    if (pending_connect_viz_on_main_thread_ && application_is_foreground_) {
      BrowserMainLoop::GetInstance()
          ->gpu_channel_establish_factory()
          ->EstablishGpuChannel(std::move(pending_connect_viz_on_main_thread_));
    }
  }

  void EnqueueLowEndBackgroundCleanup() {
    if (base::SysInfo::IsLowEndDevice()) {
      low_end_background_cleanup_task_.Reset(
          base::BindOnce(&CompositorDependencies::DoLowEndBackgroundCleanup,
                         base::Unretained(this)));
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE, low_end_background_cleanup_task_.callback(),
          base::TimeDelta::FromSeconds(5));
    }
  }

  void DoLowEndBackgroundCleanup() {
    // When we become visible, we immediately cancel the callback that runs this
    // code. First, evict all unlocked frames, allowing resources to be
    // reclaimed.
    viz::FrameEvictionManager::GetInstance()->PurgeAllUnlockedFrames();

    // Next, notify the GPU process to do background processing, which will
    // lose all renderer contexts.
    content::GpuProcessHost::CallOnIO(
        content::GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED,
        false /* force_create */,
        base::BindRepeating([](content::GpuProcessHost* host) {
          if (host) {
            host->gpu_service()->OnBackgroundCleanup();
          }
        }));
  }

  // This callback function runs when application state changes. If application
  // state is UNKNOWN, consider it as the app running as a conservative
  // approach so that we don't send the gpu services to background.
  void OnApplicationStateChange(
      base::android::ApplicationState application_state) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    switch (application_state) {
      case base::android::APPLICATION_STATE_UNKNOWN:
      case base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES:
      case base::android::APPLICATION_STATE_HAS_PAUSED_ACTIVITIES:
        if (application_is_foreground_)
          return;
        application_is_foreground_ = true;
        GpuDataManagerImpl::GetInstance()->SetApplicationVisible(true);
        BrowserGpuChannelHostFactorySetApplicationVisible(true);
        SendOnForegroundedToGpuService();
        low_end_background_cleanup_task_.Cancel();
        TryEstablishVizConnectionIfNeeded();
        break;
      case base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES:
      case base::android::APPLICATION_STATE_HAS_DESTROYED_ACTIVITIES:
        if (!application_is_foreground_)
          return;
        application_is_foreground_ = false;
        GpuDataManagerImpl::GetInstance()->SetApplicationVisible(false);
        BrowserGpuChannelHostFactorySetApplicationVisible(false);
        SendOnBackgroundedToGpuService();
        EnqueueLowEndBackgroundCleanup();
    }
  }

  // A task which runs cleanup tasks on low-end Android after a delay. Enqueued
  // when we hide, canceled when we're shown.
  base::CancelableOnceClosure low_end_background_cleanup_task_;

  // An instance of Android AppListener.
  std::unique_ptr<base::android::ApplicationStatusListener> app_listener_;
  bool application_is_foreground_ = true;
  gpu::GpuChannelEstablishedCallback pending_connect_viz_on_main_thread_;
};

const unsigned int kMaxDisplaySwapBuffers = 1U;

#if BUILDFLAG(ENABLE_VULKAN)
scoped_refptr<viz::VulkanContextProvider> GetSharedVulkanContextProvider() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableVulkan))
    return nullptr;

  scoped_refptr<viz::VulkanContextProvider>& context_provider =
      CompositorDependencies::Get().vulkan_context_provider;
  if (!context_provider) {
    std::unique_ptr<gpu::VulkanImplementation>& vulkan_implementation =
        CompositorDependencies::Get().vulkan_implementation;
    DCHECK(!vulkan_implementation);
    vulkan_implementation = gpu::CreateVulkanImplementation();
    context_provider = viz::VulkanInProcessContextProvider::Create(
        vulkan_implementation.get());
  }
  return context_provider;
}
#endif

gpu::SharedMemoryLimits GetCompositorContextSharedMemoryLimits(
    gfx::NativeWindow window) {
  const gfx::Size screen_size = display::Screen::GetScreen()
                                    ->GetDisplayNearestWindow(window)
                                    .GetSizeInPixel();
  return gpu::SharedMemoryLimits::ForDisplayCompositor(screen_size);
}

gpu::ContextCreationAttribs GetCompositorContextAttributes(
    const gfx::ColorSpace& display_color_space,
    bool requires_alpha_channel) {
  // This is used for the browser compositor (offscreen) and for the display
  // compositor (onscreen), so ask for capabilities needed by either one.
  // The default framebuffer for an offscreen context is not used, so it does
  // not need alpha, stencil, depth, antialiasing. The display compositor does
  // not use these things either, except for alpha when it has a transparent
  // background.
  gpu::ContextCreationAttribs attributes;
  attributes.alpha_size = -1;
  attributes.stencil_size = 0;
  attributes.depth_size = 0;
  attributes.samples = 0;
  attributes.sample_buffers = 0;
  attributes.bind_generates_resource = false;
  if (display_color_space == gfx::ColorSpace::CreateSRGB()) {
    attributes.color_space = gpu::COLOR_SPACE_SRGB;
  } else if (display_color_space == gfx::ColorSpace::CreateDisplayP3D65()) {
    attributes.color_space = gpu::COLOR_SPACE_DISPLAY_P3;
  } else {
    attributes.color_space = gpu::COLOR_SPACE_UNSPECIFIED;
    DLOG(ERROR) << "Android color space is neither sRGB nor P3, output color "
                   "will be incorrect.";
  }

  if (requires_alpha_channel) {
    attributes.alpha_size = 8;
  } else if (base::SysInfo::AmountOfPhysicalMemoryMB() <= 512) {
    // In this case we prefer to use RGB565 format instead of RGBA8888 if
    // possible.
    // TODO(danakj): CommandBufferStub constructor checks for alpha == 0
    // in order to enable 565, but it should avoid using 565 when -1s are
    // specified
    // (IOW check that a <= 0 && rgb > 0 && rgb <= 565) then alpha should be
    // -1.
    // TODO(liberato): This condition is memorized in ComositorView.java, to
    // avoid using two surfaces temporarily during alpha <-> no alpha
    // transitions.  If these mismatch, then we risk a power regression if the
    // SurfaceView is not marked as eOpaque (FORMAT_OPAQUE), and we have an
    // EGL surface with an alpha channel.  SurfaceFlinger needs at least one of
    // those hints to optimize out alpha blending.
    attributes.alpha_size = 0;
    attributes.red_size = 5;
    attributes.green_size = 6;
    attributes.blue_size = 5;
  }

  attributes.enable_swap_timestamps_if_supported = true;

  return attributes;
}

void CreateContextProviderAfterGpuChannelEstablished(
    gpu::SurfaceHandle handle,
    gpu::ContextCreationAttribs attributes,
    gpu::SharedMemoryLimits shared_memory_limits,
    Compositor::ContextProviderCallback callback,
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host) {
  if (!gpu_channel_host)
    callback.Run(nullptr);

  gpu::GpuChannelEstablishFactory* factory =
      BrowserMainLoop::GetInstance()->gpu_channel_establish_factory();

  int32_t stream_id = kGpuStreamIdDefault;
  gpu::SchedulingPriority stream_priority = kGpuStreamPriorityUI;

  constexpr bool automatic_flushes = false;
  constexpr bool support_locking = false;
  constexpr bool support_grcontext = false;

  auto context_provider =
      base::MakeRefCounted<ws::ContextProviderCommandBuffer>(
          std::move(gpu_channel_host), factory->GetGpuMemoryBufferManager(),
          stream_id, stream_priority, handle,
          GURL(std::string("chrome://gpu/Compositor::CreateContextProvider")),
          automatic_flushes, support_locking, support_grcontext,
          shared_memory_limits, attributes,
          ws::command_buffer_metrics::ContextType::UNKNOWN);
  callback.Run(std::move(context_provider));
}

class AndroidOutputSurface : public viz::OutputSurface {
 public:
  AndroidOutputSurface(
      scoped_refptr<ws::ContextProviderCommandBuffer> context_provider,
      base::RepeatingCallback<void(const gfx::Size&)> swap_buffers_callback)
      : viz::OutputSurface(std::move(context_provider)),
        swap_buffers_callback_(std::move(swap_buffers_callback)),
        overlay_candidate_validator_(
            new viz::CompositorOverlayCandidateValidatorAndroid()),
        weak_ptr_factory_(this) {
    capabilities_.max_frames_pending = kMaxDisplaySwapBuffers;
  }

  ~AndroidOutputSurface() override = default;

  void SwapBuffers(viz::OutputSurfaceFrame frame) override {
    auto callback =
        base::BindOnce(&AndroidOutputSurface::OnSwapBuffersCompleted,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(frame.latency_info), frame.size);
    uint32_t flags = 0;
    gpu::ContextSupport::PresentationCallback presentation_callback;
    if (frame.need_presentation_feedback) {
      flags |= gpu::SwapBuffersFlags::kPresentationFeedback;
      presentation_callback =
          base::BindOnce(&AndroidOutputSurface::OnPresentation,
                         weak_ptr_factory_.GetWeakPtr());
    }
    if (frame.sub_buffer_rect) {
      DCHECK(frame.sub_buffer_rect->IsEmpty());
      context_provider_->ContextSupport()->CommitOverlayPlanes(
          flags, std::move(callback), std::move(presentation_callback));
    } else {
      context_provider_->ContextSupport()->Swap(
          flags, std::move(callback), std::move(presentation_callback));
    }
  }

#if BUILDFLAG(ENABLE_VULKAN)
  gpu::VulkanSurface* GetVulkanSurface() override {
    NOTIMPLEMENTED();
    return nullptr;
  }
#endif

  void BindToClient(viz::OutputSurfaceClient* client) override {
    DCHECK(client);
    DCHECK(!client_);
    client_ = client;
  }

  void EnsureBackbuffer() override {}

  void DiscardBackbuffer() override {
    context_provider()->ContextGL()->DiscardBackbufferCHROMIUM();
  }

  void BindFramebuffer() override {
    context_provider()->ContextGL()->BindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  void SetDrawRectangle(const gfx::Rect& rect) override {}

  void Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha,
               bool use_stencil) override {
    context_provider()->ContextGL()->ResizeCHROMIUM(
        size.width(), size.height(), device_scale_factor,
        gl::GetGLColorSpace(color_space), has_alpha);
  }

  viz::OverlayCandidateValidator* GetOverlayCandidateValidator()
      const override {
    return overlay_candidate_validator_.get();
  }

  bool IsDisplayedAsOverlayPlane() const override { return false; }
  unsigned GetOverlayTextureId() const override { return 0; }
  gfx::BufferFormat GetOverlayBufferFormat() const override {
    return gfx::BufferFormat::RGBX_8888;
  }
  bool HasExternalStencilTest() const override { return false; }
  void ApplyExternalStencil() override {}

  uint32_t GetFramebufferCopyTextureFormat() override {
    auto* gl =
        static_cast<ws::ContextProviderCommandBuffer*>(context_provider());
    return gl->GetCopyTextureInternalFormat();
  }

  unsigned UpdateGpuFence() override { return 0; }

 private:
  gpu::CommandBufferProxyImpl* GetCommandBufferProxy() {
    ws::ContextProviderCommandBuffer* provider_command_buffer =
        static_cast<ws::ContextProviderCommandBuffer*>(context_provider_.get());
    gpu::CommandBufferProxyImpl* command_buffer_proxy =
        provider_command_buffer->GetCommandBufferProxy();
    DCHECK(command_buffer_proxy);
    return command_buffer_proxy;
  }

  void OnSwapBuffersCompleted(std::vector<ui::LatencyInfo> latency_info,
                              gfx::Size swap_size,
                              const gpu::SwapBuffersCompleteParams& params) {
    client_->DidReceiveSwapBuffersAck();
    swap_buffers_callback_.Run(swap_size);
    UpdateLatencyInfoOnSwap(params.swap_response, &latency_info);
    latency_tracker_.OnGpuSwapBuffersCompleted(latency_info);
  }

  void OnPresentation(const gfx::PresentationFeedback& feedback) {
    client_->DidReceivePresentationFeedback(feedback);
  }

 private:
  viz::OutputSurfaceClient* client_ = nullptr;
  base::RepeatingCallback<void(const gfx::Size&)> swap_buffers_callback_;
  std::unique_ptr<viz::OverlayCandidateValidator> overlay_candidate_validator_;
  ui::LatencyTracker latency_tracker_;

  base::WeakPtrFactory<AndroidOutputSurface> weak_ptr_factory_;
};

#if BUILDFLAG(ENABLE_VULKAN)
class VulkanOutputSurface : public viz::OutputSurface {
 public:
  explicit VulkanOutputSurface(
      scoped_refptr<viz::VulkanContextProvider> vulkan_context_provider,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : OutputSurface(std::move(vulkan_context_provider)),
        task_runner_(std::move(task_runner)),
        weak_ptr_factory_(this) {}

  ~VulkanOutputSurface() override { Destroy(); }

  bool Initialize(gfx::AcceleratedWidget widget) {
    DCHECK(!surface_);
    std::unique_ptr<gpu::VulkanSurface> surface(
        vulkan_context_provider()->GetVulkanImplementation()->CreateViewSurface(
            widget));
    if (!surface->Initialize(vulkan_context_provider()->GetDeviceQueue(),
                             gpu::VulkanSurface::DEFAULT_SURFACE_FORMAT)) {
      return false;
    }
    surface_ = std::move(surface);

    return true;
  }

  void BindToClient(viz::OutputSurfaceClient* client) override {
    client_ = client;
  }

  void EnsureBackbuffer() override { NOTIMPLEMENTED(); }

  void DiscardBackbuffer() override { NOTIMPLEMENTED(); }

  void BindFramebuffer() override { NOTIMPLEMENTED(); }

  void SetDrawRectangle(const gfx::Rect& rect) override { NOTIMPLEMENTED(); }

  viz::OverlayCandidateValidator* GetOverlayCandidateValidator()
      const override {
    NOTIMPLEMENTED();
    return nullptr;
  }

  bool IsDisplayedAsOverlayPlane() const override {
    NOTIMPLEMENTED();
    return false;
  }

  unsigned GetOverlayTextureId() const override {
    NOTIMPLEMENTED();
    return 0;
  }

  gfx::BufferFormat GetOverlayBufferFormat() const override {
    NOTIMPLEMENTED();
    return gfx::BufferFormat::BGRA_8888;
  }

  void Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha,
               bool use_stencil) override {
    NOTIMPLEMENTED();
  }

  bool HasExternalStencilTest() const override {
    NOTIMPLEMENTED();
    return false;
  }

  void ApplyExternalStencil() override { NOTIMPLEMENTED(); }

  uint32_t GetFramebufferCopyTextureFormat() override {
    NOTIMPLEMENTED();
    return 0;
  }

  void SwapBuffers(viz::OutputSurfaceFrame frame) override {
    surface_->SwapBuffers();
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&VulkanOutputSurface::SwapBuffersAck,
                                      weak_ptr_factory_.GetWeakPtr()));
  }

  gpu::VulkanSurface* GetVulkanSurface() override { return surface_.get(); }

  unsigned UpdateGpuFence() override {
    NOTIMPLEMENTED();
    return 0;
  }

  void Destroy() {
    if (surface_) {
      surface_->Destroy();
      surface_.reset();
    }
  }

 private:
  void SwapBuffersAck() { client_->DidReceiveSwapBuffersAck(); }

  std::unique_ptr<gpu::VulkanSurface> surface_;
  viz::OutputSurfaceClient* client_ = nullptr;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtrFactory<VulkanOutputSurface> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VulkanOutputSurface);
};
#endif

static bool g_initialized = false;

}  // anonymous namespace

// static
Compositor* Compositor::Create(CompositorClient* client,
                               gfx::NativeWindow root_window) {
  return client ? new CompositorImpl(client, root_window) : NULL;
}

// static
void Compositor::Initialize() {
  DCHECK(!CompositorImpl::IsInitialized());
  g_initialized = true;
}

// static
void Compositor::CreateContextProvider(
    gpu::SurfaceHandle handle,
    gpu::ContextCreationAttribs attributes,
    gpu::SharedMemoryLimits shared_memory_limits,
    ContextProviderCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserMainLoop::GetInstance()
      ->gpu_channel_establish_factory()
      ->EstablishGpuChannel(
          base::BindOnce(&CreateContextProviderAfterGpuChannelEstablished,
                         handle, attributes, shared_memory_limits, callback));
}

// static
viz::FrameSinkManagerImpl* CompositorImpl::GetFrameSinkManager() {
  return CompositorDependencies::Get().frame_sink_manager_impl.get();
}

// static
viz::HostFrameSinkManager* CompositorImpl::GetHostFrameSinkManager() {
  return &CompositorDependencies::Get().host_frame_sink_manager;
}

// static
viz::FrameSinkId CompositorImpl::AllocateFrameSinkId() {
  return CompositorDependencies::Get()
      .frame_sink_id_allocator.NextFrameSinkId();
}

// static
bool CompositorImpl::IsInitialized() {
  return g_initialized;
}

CompositorImpl::CompositorImpl(CompositorClient* client,
                               gfx::NativeWindow root_window)
    : frame_sink_id_(AllocateFrameSinkId()),
      resource_manager_(root_window),
      window_(NULL),
      surface_handle_(gpu::kNullSurfaceHandle),
      client_(client),
      needs_animate_(false),
      pending_frames_(0U),
      layer_tree_frame_sink_request_pending_(false),
      lock_manager_(base::ThreadTaskRunnerHandle::Get()),
      enable_surface_synchronization_(
          features::IsSurfaceSynchronizationEnabled()),
      enable_viz_(
          base::FeatureList::IsEnabled(features::kVizDisplayCompositor)),
      weak_factory_(this) {
  DCHECK(client);

  SetRootWindow(root_window);

  // Listen to display density change events and update painted device scale
  // factor accordingly.
  display::Screen::GetScreen()->AddObserver(this);
}

CompositorImpl::~CompositorImpl() {
  display::Screen::GetScreen()->RemoveObserver(this);
  DetachRootWindow();
  // Clean-up any surface references.
  SetSurface(NULL);
}

void CompositorImpl::DetachRootWindow() {
  root_window_->DetachCompositor();
  root_window_->SetLayer(nullptr);
}

ui::UIResourceProvider& CompositorImpl::GetUIResourceProvider() {
  return *this;
}

ui::ResourceManager& CompositorImpl::GetResourceManager() {
  return resource_manager_;
}

void CompositorImpl::SetRootWindow(gfx::NativeWindow root_window) {
  DCHECK(root_window);
  DCHECK(!root_window->GetLayer());

  // TODO(mthiesse): Right now we only support swapping the root window without
  // a surface. If we want to support swapping with a surface we need to
  // handle visibility, swapping begin frame sources, etc.
  // These checks ensure we have no begin frame source, and that we don't need
  // to register one on the new window.
  DCHECK(!display_);
  DCHECK(!window_);

  scoped_refptr<cc::Layer> root_layer;
  if (root_window_) {
    root_layer = root_window_->GetLayer();
    DetachRootWindow();
  }

  root_window_ = root_window;
  root_window_->SetLayer(root_layer ? root_layer : cc::Layer::Create());
  root_window_->GetLayer()->SetBounds(size_);
  if (!readback_layer_tree_) {
    readback_layer_tree_ = cc::Layer::Create();
    readback_layer_tree_->SetHideLayerAndSubtree(true);
  }
  root_window->GetLayer()->AddChild(readback_layer_tree_);
  root_window->AttachCompositor(this);
  if (!host_) {
    CreateLayerTreeHost();
    resource_manager_.Init(host_->GetUIResourceManager());
  }
  host_->SetRootLayer(root_window_->GetLayer());
  host_->SetViewportSizeAndScale(size_, root_window_->GetDipScale(),
                                 GenerateLocalSurfaceId(),
                                 GetLocalSurfaceIdAllocationTime());
}

void CompositorImpl::SetRootLayer(scoped_refptr<cc::Layer> root_layer) {
  if (subroot_layer_.get()) {
    subroot_layer_->RemoveFromParent();
    subroot_layer_ = nullptr;
  }
  if (root_window_->GetLayer()) {
    subroot_layer_ = root_window_->GetLayer();
    subroot_layer_->AddChild(root_layer);
  }
}

void CompositorImpl::SetSurface(jobject surface) {
  JNIEnv* env = base::android::AttachCurrentThread();
  gpu::GpuSurfaceTracker* tracker = gpu::GpuSurfaceTracker::Get();

  if (window_) {
    // Shut down GL context before unregistering surface.
    SetVisible(false);
    tracker->RemoveSurface(surface_handle_);
    ANativeWindow_release(window_);
    window_ = NULL;
    surface_handle_ = gpu::kNullSurfaceHandle;
  }

  ANativeWindow* window = NULL;
  if (surface) {
    // Note: This ensures that any local references used by
    // ANativeWindow_fromSurface are released immediately. This is needed as a
    // workaround for https://code.google.com/p/android/issues/detail?id=68174
    base::android::ScopedJavaLocalFrame scoped_local_reference_frame(env);
    window = ANativeWindow_fromSurface(env, surface);
  }

  if (window) {
    window_ = window;
    ANativeWindow_acquire(window);
    // Register first, SetVisible() might create a LayerTreeFrameSink.
    surface_handle_ = tracker->AddSurfaceForNativeWidget(
        gpu::GpuSurfaceTracker::SurfaceRecord(window, surface));
    SetVisible(true);
    ANativeWindow_release(window);
  }
}

void CompositorImpl::SetBackgroundColor(int color) {
  DCHECK(host_);
  host_->set_background_color(color);
}

void CompositorImpl::CreateLayerTreeHost() {
  DCHECK(!host_);

  cc::LayerTreeSettings settings;
  settings.use_zero_copy = true;
  settings.enable_surface_synchronization = enable_surface_synchronization_;
  settings.build_hit_test_data = features::IsVizHitTestingSurfaceLayerEnabled();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  settings.initial_debug_state.SetRecordRenderingStats(
      command_line->HasSwitch(cc::switches::kEnableGpuBenchmarking));
  settings.initial_debug_state.show_fps_counter =
      command_line->HasSwitch(cc::switches::kUIShowFPSCounter);
  if (command_line->HasSwitch(cc::switches::kUIShowCompositedLayerBorders))
    settings.initial_debug_state.show_debug_borders.set();
  settings.single_thread_proxy_scheduler = true;
  settings.use_painted_device_scale_factor = true;

  animation_host_ = cc::AnimationHost::CreateMainInstance();

  cc::LayerTreeHost::InitParams params;
  params.client = this;
  params.task_graph_runner = &CompositorDependencies::Get().task_graph_runner;
  params.main_task_runner = base::ThreadTaskRunnerHandle::Get();
  params.settings = &settings;
  params.mutator_host = animation_host_.get();
  host_ = cc::LayerTreeHost::CreateSingleThreaded(this, std::move(params));
  DCHECK(!host_->IsVisible());
  host_->SetViewportSizeAndScale(size_, root_window_->GetDipScale(),
                                 GenerateLocalSurfaceId(),
                                 GetLocalSurfaceIdAllocationTime());

  if (needs_animate_)
    host_->SetNeedsAnimate();
}

void CompositorImpl::SetVisible(bool visible) {
  TRACE_EVENT1("cc", "CompositorImpl::SetVisible", "visible", visible);

  if (!visible) {
    DCHECK(host_->IsVisible());
    // Tear down the display first, synchronously completing any pending
    // draws/readbacks if poosible.
    TearDownDisplayAndUnregisterRootFrameSink();
    // Hide the LayerTreeHost and release its frame sink.
    host_->SetVisible(false);
    host_->ReleaseLayerTreeFrameSink();
    has_layer_tree_frame_sink_ = false;
    pending_frames_ = 0;
  } else {
    DCHECK(!host_->IsVisible());
    RegisterRootFrameSink();
    host_->SetVisible(true);
    has_submitted_frame_since_became_visible_ = false;
    if (layer_tree_frame_sink_request_pending_)
      HandlePendingLayerTreeFrameSinkRequest();
  }
}

void CompositorImpl::TearDownDisplayAndUnregisterRootFrameSink() {
  if (enable_viz_) {
    // Make a best effort to try to complete pending readbacks.
    // TODO(crbug.com/637035): Consider doing this in a better way,
    // ideally with the guarantee of readbacks completing.
    if (display_private_ && HavePendingReadbacks()) {
      // Note that while this is not a Sync IPC, the call to
      // InvalidateFrameSinkId below will end up triggering a sync call to
      // FrameSinkManager::DestroyCompositorFrameSink, as this is the root
      // frame sink. Because |display_private_| is an associated interface to
      // FrameSinkManager, this subsequent sync call will ensure ordered
      // execution of this call.
      display_private_->ForceImmediateDrawAndSwapIfPossible();
    }

    GetHostFrameSinkManager()->InvalidateFrameSinkId(frame_sink_id_);
    display_private_.reset();
  } else {
    // Make a best effort to try to complete pending readbacks.
    // TODO(crbug.com/637035): Consider doing this in a better way,
    // ideally with the guarantee of readbacks completing.
    if (display_ && HavePendingReadbacks())
      display_->ForceImmediateDrawAndSwapIfPossible();

    if (display_) {
      GetFrameSinkManager()->UnregisterBeginFrameSource(
          root_window_->GetBeginFrameSource());
    }

    GetHostFrameSinkManager()->InvalidateFrameSinkId(frame_sink_id_);
    display_.reset();
  }
}

void CompositorImpl::RegisterRootFrameSink() {
  GetHostFrameSinkManager()->RegisterFrameSinkId(
      frame_sink_id_, this, viz::ReportFirstSurfaceActivation::kNo);
  GetHostFrameSinkManager()->SetFrameSinkDebugLabel(frame_sink_id_,
                                                    "CompositorImpl");
}

void CompositorImpl::SetWindowBounds(const gfx::Size& size) {
  if (size_ == size)
    return;

  size_ = size;
  if (host_) {
    // TODO(ccameron): Ensure a valid LocalSurfaceId here.
    host_->SetViewportSizeAndScale(size_, root_window_->GetDipScale(),
                                   GenerateLocalSurfaceId(),
                                   GetLocalSurfaceIdAllocationTime());
  }
  if (display_)
    display_->Resize(size);

  if (display_private_)
    display_private_->Resize(size);

  root_window_->GetLayer()->SetBounds(size);
}

void CompositorImpl::SetRequiresAlphaChannel(bool flag) {
  requires_alpha_channel_ = flag;
}

void CompositorImpl::SetNeedsComposite() {
  if (!host_->IsVisible())
    return;
  TRACE_EVENT0("compositor", "Compositor::SetNeedsComposite");
  host_->SetNeedsAnimate();
}

void CompositorImpl::UpdateLayerTreeHost() {
  client_->UpdateLayerTreeHost();
  if (needs_animate_) {
    needs_animate_ = false;
    root_window_->Animate(base::TimeTicks::Now());
  }
}

void CompositorImpl::RequestNewLayerTreeFrameSink() {
  DCHECK(!layer_tree_frame_sink_request_pending_)
      << "LayerTreeFrameSink request is already pending?";

  layer_tree_frame_sink_request_pending_ = true;
  HandlePendingLayerTreeFrameSinkRequest();
}

void CompositorImpl::DidInitializeLayerTreeFrameSink() {
  layer_tree_frame_sink_request_pending_ = false;
  has_layer_tree_frame_sink_ = true;
  for (auto& frame_sink_id : pending_child_frame_sink_ids_)
    AddChildFrameSink(frame_sink_id);

  pending_child_frame_sink_ids_.clear();
}

void CompositorImpl::DidFailToInitializeLayerTreeFrameSink() {
  // The context is bound/initialized before handing it to the
  // LayerTreeFrameSink.
  NOTREACHED();
}

void CompositorImpl::HandlePendingLayerTreeFrameSinkRequest() {
  DCHECK(layer_tree_frame_sink_request_pending_);

  // We might have been made invisible now.
  if (!host_->IsVisible())
    return;

#if BUILDFLAG(ENABLE_VULKAN)
  if (CreateVulkanOutputSurface())
    return;
#endif

  DCHECK(surface_handle_ != gpu::kNullSurfaceHandle);
  BrowserMainLoop::GetInstance()
      ->gpu_channel_establish_factory()
      ->EstablishGpuChannel(
          base::BindOnce(&CompositorImpl::OnGpuChannelEstablished,
                         weak_factory_.GetWeakPtr()));
}

#if BUILDFLAG(ENABLE_VULKAN)
bool CompositorImpl::CreateVulkanOutputSurface() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableVulkan))
    return false;

  scoped_refptr<viz::VulkanContextProvider> vulkan_context_provider =
      GetSharedVulkanContextProvider();
  if (!vulkan_context_provider)
    return false;

  // TODO(crbug.com/582558): Need to match GL and implement DidSwapBuffers.
  auto vulkan_surface = std::make_unique<VulkanOutputSurface>(
      vulkan_context_provider, base::ThreadTaskRunnerHandle::Get());
  if (!vulkan_surface->Initialize(window_))
    return false;

  InitializeDisplay(std::move(vulkan_surface), nullptr);

  return !!display_;
}
#endif

void CompositorImpl::OnGpuChannelEstablished(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host) {
  // We might end up queing multiple GpuChannel requests for the same
  // LayerTreeFrameSink request as the visibility of the compositor changes, so
  // the LayerTreeFrameSink request could have been handled already.
  if (!layer_tree_frame_sink_request_pending_)
    return;

  if (!gpu_channel_host) {
    HandlePendingLayerTreeFrameSinkRequest();
    return;
  }

  // We don't need the context anymore if we are invisible.
  if (!host_->IsVisible()) {
    return;
  }

  DCHECK(window_);
  DCHECK_NE(surface_handle_, gpu::kNullSurfaceHandle);

  gpu::GpuChannelEstablishFactory* factory =
      BrowserMainLoop::GetInstance()->gpu_channel_establish_factory();

  int32_t stream_id = kGpuStreamIdDefault;
  gpu::SchedulingPriority stream_priority = kGpuStreamPriorityUI;

  constexpr bool support_locking = false;
  constexpr bool automatic_flushes = false;
  constexpr bool support_grcontext = true;
  display_color_space_ = display::Screen::GetScreen()
                             ->GetDisplayNearestWindow(root_window_)
                             .color_space();
  gpu::SurfaceHandle surface_handle =
      enable_viz_ ? gpu::kNullSurfaceHandle : surface_handle_;
  auto context_provider =
      base::MakeRefCounted<ws::ContextProviderCommandBuffer>(
          std::move(gpu_channel_host), factory->GetGpuMemoryBufferManager(),
          stream_id, stream_priority, surface_handle,
          GURL(std::string("chrome://gpu/CompositorImpl::") +
               std::string("CompositorContextProvider")),
          automatic_flushes, support_locking, support_grcontext,
          GetCompositorContextSharedMemoryLimits(root_window_),
          GetCompositorContextAttributes(display_color_space_,
                                         requires_alpha_channel_),
          ws::command_buffer_metrics::ContextType::BROWSER_COMPOSITOR);
  auto result = context_provider->BindToCurrentThread();
  if (result != gpu::ContextResult::kSuccess) {
    if (gpu::IsFatalOrSurfaceFailure(result))
      OnFatalOrSurfaceContextCreationFailure(result);
    HandlePendingLayerTreeFrameSinkRequest();
    return;
  }

  if (enable_viz_) {
    InitializeVizLayerTreeFrameSink(std::move(context_provider));
  } else {
    // Unretained is safe this owns viz::Display which owns OutputSurface.
    auto display_output_surface = std::make_unique<AndroidOutputSurface>(
        context_provider, base::BindRepeating(&CompositorImpl::DidSwapBuffers,
                                              base::Unretained(this)));
    InitializeDisplay(std::move(display_output_surface),
                      std::move(context_provider));
  }
}

void CompositorImpl::InitializeDisplay(
    std::unique_ptr<viz::OutputSurface> display_output_surface,
    scoped_refptr<viz::ContextProvider> context_provider) {
  DCHECK(layer_tree_frame_sink_request_pending_);

  pending_frames_ = 0;

  if (context_provider) {
    gpu_capabilities_ = context_provider->ContextCapabilities();
  } else {
    // TODO(danakj): Populate gpu_capabilities_ for VulkanContextProvider.
  }

  viz::FrameSinkManagerImpl* manager = GetFrameSinkManager();
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::ThreadTaskRunnerHandle::Get();
  auto scheduler = std::make_unique<viz::DisplayScheduler>(
      root_window_->GetBeginFrameSource(), task_runner.get(),
      display_output_surface->capabilities().max_frames_pending);

  viz::RendererSettings renderer_settings;
  renderer_settings.allow_antialiasing = false;
  renderer_settings.highp_threshold_min = 2048;
  renderer_settings.auto_resize_output_surface = false;
  renderer_settings.initial_screen_size =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(root_window_)
          .GetSizeInPixel();
  auto* gpu_memory_buffer_manager = BrowserMainLoop::GetInstance()
                                        ->gpu_channel_establish_factory()
                                        ->GetGpuMemoryBufferManager();

  // Don't re-register BeginFrameSource on context loss.
  const bool should_register_begin_frame_source = !display_;

  display_ = std::make_unique<viz::Display>(
      nullptr, renderer_settings, frame_sink_id_,
      std::move(display_output_surface), std::move(scheduler), task_runner);

  auto layer_tree_frame_sink = std::make_unique<viz::DirectLayerTreeFrameSink>(
      frame_sink_id_, GetHostFrameSinkManager(), manager, display_.get(),
      nullptr /* display_client */, context_provider,
      nullptr /* worker_context_provider */, task_runner,
      gpu_memory_buffer_manager, features::IsVizHitTestingEnabled());

  display_->SetVisible(true);
  display_->Resize(size_);
  display_->SetColorSpace(display_color_space_, display_color_space_);
  if (should_register_begin_frame_source) {
    GetFrameSinkManager()->RegisterBeginFrameSource(
        root_window_->GetBeginFrameSource(), frame_sink_id_);
  }
  host_->SetLayerTreeFrameSink(std::move(layer_tree_frame_sink));
}

void CompositorImpl::DidSwapBuffers(const gfx::Size& swap_size) {
  client_->DidSwapBuffers(swap_size);

  if (swap_completed_with_size_for_testing_)
    swap_completed_with_size_for_testing_.Run(swap_size);
}

cc::UIResourceId CompositorImpl::CreateUIResource(
    cc::UIResourceClient* client) {
  TRACE_EVENT0("compositor", "CompositorImpl::CreateUIResource");
  return host_->GetUIResourceManager()->CreateUIResource(client);
}

void CompositorImpl::DeleteUIResource(cc::UIResourceId resource_id) {
  TRACE_EVENT0("compositor", "CompositorImpl::DeleteUIResource");
  host_->GetUIResourceManager()->DeleteUIResource(resource_id);
}

bool CompositorImpl::SupportsETC1NonPowerOfTwo() const {
  return gpu_capabilities_.texture_format_etc1_npot;
}

void CompositorImpl::DidSubmitCompositorFrame() {
  TRACE_EVENT0("compositor", "CompositorImpl::DidSubmitCompositorFrame");
  pending_frames_++;
  has_submitted_frame_since_became_visible_ = true;
}

void CompositorImpl::DidReceiveCompositorFrameAck() {
  TRACE_EVENT0("compositor", "CompositorImpl::DidReceiveCompositorFrameAck");
  DCHECK_GT(pending_frames_, 0U);
  pending_frames_--;
  client_->DidSwapFrame(pending_frames_);
}

void CompositorImpl::DidLoseLayerTreeFrameSink() {
  TRACE_EVENT0("compositor", "CompositorImpl::DidLoseLayerTreeFrameSink");
  has_layer_tree_frame_sink_ = false;
  client_->DidSwapFrame(0);
}

void CompositorImpl::DidCommit() {
  root_window_->OnCompositingDidCommit();
}

void CompositorImpl::AttachLayerForReadback(scoped_refptr<cc::Layer> layer) {
  readback_layer_tree_->AddChild(layer);
}

void CompositorImpl::RequestCopyOfOutputOnRootLayer(
    std::unique_ptr<viz::CopyOutputRequest> request) {
  root_window_->GetLayer()->RequestCopyOfOutput(std::move(request));
}

void CompositorImpl::SetNeedsAnimate() {
  needs_animate_ = true;
  if (!host_->IsVisible())
    return;

  TRACE_EVENT0("compositor", "Compositor::SetNeedsAnimate");
  host_->SetNeedsAnimate();
}

viz::FrameSinkId CompositorImpl::GetFrameSinkId() {
  return frame_sink_id_;
}

void CompositorImpl::AddChildFrameSink(const viz::FrameSinkId& frame_sink_id) {
  if (has_layer_tree_frame_sink_) {
    GetHostFrameSinkManager()->RegisterFrameSinkHierarchy(frame_sink_id_,
                                                          frame_sink_id);
  } else {
    pending_child_frame_sink_ids_.insert(frame_sink_id);
  }
}

void CompositorImpl::RemoveChildFrameSink(
    const viz::FrameSinkId& frame_sink_id) {
  auto it = pending_child_frame_sink_ids_.find(frame_sink_id);
  if (it != pending_child_frame_sink_ids_.end()) {
    pending_child_frame_sink_ids_.erase(it);
    return;
  }
  GetHostFrameSinkManager()->UnregisterFrameSinkHierarchy(frame_sink_id_,
                                                          frame_sink_id);
}

void CompositorImpl::OnDisplayMetricsChanged(const display::Display& display,
                                             uint32_t changed_metrics) {
  if (changed_metrics & display::DisplayObserver::DisplayMetric::
                            DISPLAY_METRIC_DEVICE_SCALE_FACTOR &&
      display.id() == display::Screen::GetScreen()
                          ->GetDisplayNearestWindow(root_window_)
                          .id()) {
    // TODO(ccameron): This is transiently incorrect -- |size_| must be
    // recalculated here as well. Is the call in SetWindowBounds sufficient?
    host_->SetViewportSizeAndScale(size_, root_window_->GetDipScale(),
                                   GenerateLocalSurfaceId(),
                                   GetLocalSurfaceIdAllocationTime());
  }
}

bool CompositorImpl::HavePendingReadbacks() {
  return !readback_layer_tree_->children().empty();
}

std::unique_ptr<ui::CompositorLock> CompositorImpl::GetCompositorLock(
    ui::CompositorLockClient* client,
    base::TimeDelta timeout) {
  std::unique_ptr<cc::ScopedDeferCommits> scoped_defer_commits =
      host_ ? host_->DeferCommits() : nullptr;
  return lock_manager_.GetCompositorLock(client, timeout,
                                         std::move(scoped_defer_commits));
}

bool CompositorImpl::IsDrawingFirstVisibleFrame() const {
  return !has_submitted_frame_since_became_visible_;
}

void CompositorImpl::SetVSyncPaused(bool paused) {
  // No action needed in non-Viz mode, as VSync is handled in WindowAndroid.
  if (!enable_viz_)
    return;

  if (vsync_paused_ == paused)
    return;

  vsync_paused_ = paused;
  if (display_private_)
    display_private_->SetVSyncPaused(paused);
}

void CompositorImpl::InitializeVizLayerTreeFrameSink(
    scoped_refptr<ws::ContextProviderCommandBuffer> context_provider) {
  DCHECK(enable_viz_);

  pending_frames_ = 0;
  gpu_capabilities_ = context_provider->ContextCapabilities();
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::ThreadTaskRunnerHandle::Get();

  auto root_params = viz::mojom::RootCompositorFrameSinkParams::New();

  // Android requires swap size notifications.
  root_params->send_swap_size_notifications = true;

  // Create interfaces for a root CompositorFrameSink.
  viz::mojom::CompositorFrameSinkAssociatedPtrInfo sink_info;
  root_params->compositor_frame_sink = mojo::MakeRequest(&sink_info);
  viz::mojom::CompositorFrameSinkClientRequest client_request =
      mojo::MakeRequest(&root_params->compositor_frame_sink_client);
  root_params->display_private = mojo::MakeRequest(&display_private_);
  display_client_ = std::make_unique<AndroidHostDisplayClient>(
      base::BindRepeating(&CompositorImpl::DidSwapBuffers,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(
          &CompositorImpl::OnFatalOrSurfaceContextCreationFailure,
          weak_factory_.GetWeakPtr()));
  root_params->display_client =
      display_client_->GetBoundPtr(task_runner).PassInterface();

  viz::RendererSettings renderer_settings;
  renderer_settings.allow_antialiasing = false;
  renderer_settings.highp_threshold_min = 2048;
  renderer_settings.requires_alpha_channel = requires_alpha_channel_;
  renderer_settings.initial_screen_size =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(root_window_)
          .GetSizeInPixel();
  root_params->frame_sink_id = frame_sink_id_;
  root_params->widget = surface_handle_;
  root_params->gpu_compositing = true;
  root_params->renderer_settings = renderer_settings;

  GetHostFrameSinkManager()->CreateRootCompositorFrameSink(
      std::move(root_params));

  // Create LayerTreeFrameSink with the browser end of CompositorFrameSink.
  cc::mojo_embedder::AsyncLayerTreeFrameSink::InitParams params;
  params.compositor_task_runner = task_runner;
  params.gpu_memory_buffer_manager = BrowserMainLoop::GetInstance()
                                         ->gpu_channel_establish_factory()
                                         ->GetGpuMemoryBufferManager();
  params.pipes.compositor_frame_sink_associated_info = std::move(sink_info);
  params.pipes.client_request = std::move(client_request);
  params.local_surface_id_provider =
      std::make_unique<viz::DefaultLocalSurfaceIdProvider>();
  params.enable_surface_synchronization = true;
  params.hit_test_data_provider =
      std::make_unique<viz::HitTestDataProviderDrawQuad>(
          false /* should_ask_for_child_region */,
          true /* root_accepts_events */);
  params.client_name = kBrowser;
  auto layer_tree_frame_sink =
      std::make_unique<cc::mojo_embedder::AsyncLayerTreeFrameSink>(
          std::move(context_provider), nullptr, &params);
  host_->SetLayerTreeFrameSink(std::move(layer_tree_frame_sink));
  display_private_->SetDisplayVisible(true);
  display_private_->Resize(size_);
  display_private_->SetVSyncPaused(vsync_paused_);
}

viz::LocalSurfaceId CompositorImpl::GenerateLocalSurfaceId() const {
  if (enable_surface_synchronization_)
    return CompositorDependencies::Get().surface_id_allocator.GenerateId();

  return viz::LocalSurfaceId();
}

base::TimeTicks CompositorImpl::GetLocalSurfaceIdAllocationTime() const {
  if (enable_surface_synchronization_)
    return CompositorDependencies::Get().surface_id_allocator.allocation_time();
  return base::TimeTicks();
}

void CompositorImpl::OnFatalOrSurfaceContextCreationFailure(
    gpu::ContextResult context_result) {
  DCHECK(gpu::IsFatalOrSurfaceFailure(context_result));
  LOG_IF(FATAL, context_result == gpu::ContextResult::kFatalFailure)
      << "Fatal error making Gpu context";

  if (context_result == gpu::ContextResult::kSurfaceFailure) {
    SetSurface(nullptr);
    client_->RecreateSurface();
  }
}

void CompositorImpl::OnFirstSurfaceActivation(const viz::SurfaceInfo& info) {
  NOTREACHED();
}

}  // namespace content
