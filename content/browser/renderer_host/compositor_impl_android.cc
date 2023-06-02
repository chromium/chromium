// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/compositor_impl_android.h"

#include <android/bitmap.h>
#include <stdint.h>

#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/android/application_status_listener.h"
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/lock.h"
#include "base/task/common/task_annotator.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "cc/animation/animation_host.h"
#include "cc/base/switches.h"
#include "cc/input/input_handler.h"
#include "cc/layers/layer.h"
#include "cc/metrics/begin_main_frame_metrics.h"
#include "cc/metrics/web_vital_metrics.h"
#include "cc/mojo_embedder/async_layer_tree_frame_sink.h"
#include "cc/resources/ui_resource_manager.h"
#include "cc/slim/frame_sink.h"
#include "cc/slim/layer.h"
#include "cc/slim/layer_tree.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_settings.h"
#include "components/viz/common/features.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/surface_range.h"
#include "components/viz/common/viz_utils.h"
#include "components/viz/host/host_display_client.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/renderer_host/compositor_dependencies_android.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/android/compositor_client.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/gpu_stream_constants.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/command_buffer/common/swap_buffers_flags.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/gpu_surface_tracker.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkMallocPixelRef.h"
#include "ui/android/window_android.h"
#include "ui/display/display.h"
#include "ui/display/display_transform.h"
#include "ui/display/screen.h"
#include "ui/gfx/ca_layer_params.h"
#include "ui/gfx/swap_result.h"
#include "ui/latency/latency_tracker.h"

namespace content {

namespace {

// NOINLINE to make sure crashes use this for magic signature.
NOINLINE void FatalSurfaceFailure() {
  LOG(FATAL) << "Fatal surface initialization failure";
}

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
  attributes.color_space = gpu::COLOR_SPACE_SRGB;

  if (requires_alpha_channel) {
    attributes.alpha_size = 8;
  } else if (viz::PreferRGB565ResourcesForDisplay()) {
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
  if (!gpu_channel_host) {
    std::move(callback).Run(nullptr);
    return;
  }

  gpu::GpuChannelEstablishFactory* factory =
      BrowserGpuChannelHostFactory::instance();

  int32_t stream_id = kGpuStreamIdDefault;
  gpu::SchedulingPriority stream_priority = kGpuStreamPriorityUI;

  constexpr bool automatic_flushes = false;
  constexpr bool support_locking = false;
  constexpr bool support_grcontext = false;

  auto context_provider =
      base::MakeRefCounted<viz::ContextProviderCommandBuffer>(
          std::move(gpu_channel_host), factory->GetGpuMemoryBufferManager(),
          stream_id, stream_priority, handle,
          GURL(std::string("chrome://gpu/Compositor::CreateContextProvider")),
          automatic_flushes, support_locking, support_grcontext,
          shared_memory_limits, attributes,
          viz::command_buffer_metrics::ContextType::UNKNOWN);
  std::move(callback).Run(std::move(context_provider));
}

static bool g_initialized = false;

}  // anonymous namespace

// An implementation of HostDisplayClient which handles swap callbacks.
class CompositorImpl::AndroidHostDisplayClient : public viz::HostDisplayClient {
 public:
  explicit AndroidHostDisplayClient(CompositorImpl* compositor)
      : HostDisplayClient(gfx::kNullAcceleratedWidget),
        compositor_(compositor) {}

  // viz::mojom::DisplayClient implementation:
  void DidCompleteSwapWithSize(const gfx::Size& pixel_size) override {
    compositor_->DidSwapBuffers(pixel_size);
  }
  void OnContextCreationResult(gpu::ContextResult context_result) override {
    compositor_->OnContextCreationResult(context_result);
  }
  void SetWideColorEnabled(bool enabled) override {
    // TODO(cblume): Add support for multiple compositors.
    // If one goes wide, all should go wide.
    if (compositor_->root_window_) {
      compositor_->root_window_->SetWideColorEnabled(enabled);
    }
  }
  void SetPreferredRefreshRate(float refresh_rate) override {
    if (compositor_->root_window_) {
      compositor_->root_window_->SetPreferredRefreshRate(refresh_rate);
    }
  }

 private:
  raw_ptr<CompositorImpl> compositor_;
};

class CompositorImpl::HostBeginFrameObserver
    : public viz::mojom::BeginFrameObserver {
 public:
  HostBeginFrameObserver(
      const base::flat_set<SimpleBeginFrameObserver*>& observers,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : simple_begin_frame_observers_(observers),
        task_runner_(std::move(task_runner)) {}

  void OnStandaloneBeginFrame(const viz::BeginFrameArgs& args) override {
    // Mark the current task as interesting, as it maybe be responsible for
    // handling input events for flings.
    base::TaskAnnotator::MarkCurrentTaskAsInterestingForTracing();
    if (args.type == viz::BeginFrameArgs::MISSED) {
      return;
    }

    if (pending_coalesce_callback_) {
      begin_frame_args_ = args;
      return;
    }

    if ((base::TimeTicks::Now() - args.frame_time) > args.interval) {
      begin_frame_args_ = args;
      pending_coalesce_callback_ = true;
      task_runner_->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(
              &CompositorImpl::HostBeginFrameObserver::CoalescedBeginFrame,
              weak_factory_.GetWeakPtr()),
          base::Microseconds(1));
      return;
    }

    CallObservers(args);
  }

  mojo::PendingRemote<viz::mojom::BeginFrameObserver> GetBoundRemote() {
    return receiver_.BindNewPipeAndPassRemote(task_runner_);
  }

 private:
  void CoalescedBeginFrame() {
    DCHECK(begin_frame_args_.IsValid());
    pending_coalesce_callback_ = false;
    viz::BeginFrameArgs args = begin_frame_args_;
    begin_frame_args_ = viz::BeginFrameArgs();
    CallObservers(args);
  }

  // This may be deleted as part of `CallObservers`.
  void CallObservers(const viz::BeginFrameArgs& args) {
    auto observers_copy = *simple_begin_frame_observers_;
    for (auto* simple_observer : observers_copy) {
      simple_observer->OnBeginFrame(args.frame_time);
    }
  }

  const raw_ref<const base::flat_set<SimpleBeginFrameObserver*>>
      simple_begin_frame_observers_;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  bool pending_coalesce_callback_ = false;
  viz::BeginFrameArgs begin_frame_args_;

  mojo::Receiver<viz::mojom::BeginFrameObserver> receiver_{this};
  base::WeakPtrFactory<HostBeginFrameObserver> weak_factory_{this};
};

class CompositorImpl::ScopedCachedBackBuffer {
 public:
  ScopedCachedBackBuffer(const viz::FrameSinkId& root_sink_id) {
    cache_id_ =
        GetHostFrameSinkManager()->CacheBackBufferForRootSink(root_sink_id);
  }
  ~ScopedCachedBackBuffer() {
    GetHostFrameSinkManager()->EvictCachedBackBuffer(cache_id_);
  }

 private:
  uint32_t cache_id_;
};

class CompositorImpl::ReadbackRefImpl
    : public ui::WindowAndroidCompositor::ReadbackRef {
 public:
  ReadbackRefImpl(base::WeakPtr<CompositorImpl> weakptr,
                  const viz::SurfaceId& surface_id_to_copy);
  ~ReadbackRefImpl() override;

 private:
  base::WeakPtr<CompositorImpl> compositor_weakptr_;
  std::unique_ptr<cc::slim::LayerTree::ScopedKeepSurfaceAlive> keep_alive_;
};

CompositorImpl::ReadbackRefImpl::ReadbackRefImpl(
    base::WeakPtr<CompositorImpl> weakptr,
    const viz::SurfaceId& surface_id_to_copy)
    : compositor_weakptr_(std::move(weakptr)) {
  DCHECK(compositor_weakptr_);
  DCHECK(compositor_weakptr_->host_);
  keep_alive_ = compositor_weakptr_->host_->CreateScopedKeepSurfaceAlive(
      surface_id_to_copy);
}

CompositorImpl::ReadbackRefImpl::~ReadbackRefImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (compositor_weakptr_) {
    compositor_weakptr_->DecrementPendingReadbacks();
  }
}

// static
Compositor* Compositor::Create(CompositorClient* client,
                               gfx::NativeWindow root_window) {
  return client ? new CompositorImpl(client, root_window) : nullptr;
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
  BrowserGpuChannelHostFactory::instance()->EstablishGpuChannel(
      base::BindOnce(&CreateContextProviderAfterGpuChannelEstablished, handle,
                     attributes, shared_memory_limits, std::move(callback)));
}

// static
bool CompositorImpl::IsInitialized() {
  return g_initialized;
}

CompositorImpl::CompositorImpl(CompositorClient* client,
                               gfx::NativeWindow root_window)
    : frame_sink_id_(AllocateFrameSinkId()),
      resource_manager_(root_window),
      window_(nullptr),
      surface_handle_(gpu::kNullSurfaceHandle),
      client_(client),
      needs_animate_(false),
      pending_frames_(0U),
      layer_tree_frame_sink_request_pending_(false),
      lock_manager_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
  DCHECK(client);

  SetRootWindow(root_window);
}

CompositorImpl::~CompositorImpl() {
  DetachRootWindow();
  // Clean-up any surface references.
  SetSurface(nullptr, false);

  BrowserGpuChannelHostFactory::instance()->MaybeCloseChannel();
}

void CompositorImpl::DetachRootWindow() {
  root_window_->DetachCompositor();
  root_window_->SetLayer(nullptr);
}

base::WeakPtr<ui::UIResourceProvider> CompositorImpl::GetUIResourceProvider() {
  return weak_factory_.GetWeakPtr();
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
  DCHECK(!window_);

  scoped_refptr<cc::slim::Layer> root_layer;
  if (root_window_) {
    root_layer = root_window_->GetLayer();
    DetachRootWindow();
  }

  root_window_ = root_window;
  root_window_->SetLayer(root_layer ? root_layer : cc::slim::Layer::Create());
  root_window_->GetLayer()->SetBounds(size_);
  root_window->AttachCompositor(this);
  if (!host_) {
    CreateLayerTreeHost();
    resource_manager_.Init(host_->GetUIResourceManager());
  }
  OnUpdateOverlayTransform();
  host_->SetRoot(root_window_->GetLayer());
  host_->SetViewportRectAndScale(gfx::Rect(size_), root_window_->GetDipScale(),
                                 GenerateLocalSurfaceId());
}

void CompositorImpl::SetRootLayer(scoped_refptr<cc::slim::Layer> root_layer) {
  if (subroot_layer_) {
    subroot_layer_->RemoveFromParent();
    subroot_layer_ = nullptr;
  }
  if (root_layer && root_window_->GetLayer()) {
    subroot_layer_ = root_window_->GetLayer();
    subroot_layer_->AddChild(root_layer);
  }
}

void CompositorImpl::SetSurface(const base::android::JavaRef<jobject>& surface,
                                bool can_be_used_with_surface_control) {
  gpu::GpuSurfaceTracker* tracker = gpu::GpuSurfaceTracker::Get();

  if (window_) {
    // Shut down GL context before unregistering surface.
    SetVisible(false);
    tracker->RemoveSurface(surface_handle_);
    window_ = nullptr;
    surface_handle_ = gpu::kNullSurfaceHandle;
  }

  gl::ScopedJavaSurface scoped_surface(surface, /*auto_release=*/false);
  gl::ScopedANativeWindow window(scoped_surface);

  if (window) {
    window_ = std::move(window);
    // Register first, SetVisible() might create a LayerTreeFrameSink.
    surface_handle_ = tracker->AddSurfaceForNativeWidget(
        gpu::GpuSurfaceTracker::SurfaceRecord(
            std::move(scoped_surface), can_be_used_with_surface_control));
    SetVisible(true);
  }
}

void CompositorImpl::SetBackgroundColor(int color) {
  DCHECK(host_);
  host_->set_background_color(SkColor4f::FromColor(color));
}

void CompositorImpl::CreateLayerTreeHost() {
  DCHECK(!host_);

  cc::slim::LayerTree::InitParams init_params;
  init_params.client = this;
  init_params.cc_task_graph_runner =
      CompositorDependenciesAndroid::Get().GetTaskGraphRunner();
  init_params.task_runner =
      content::GetUIThreadTaskRunner({BrowserTaskType::kUserInput});
  host_ = cc::slim::LayerTree::Create(std::move(init_params));
  DCHECK(!host_->IsVisible());
  host_->SetViewportRectAndScale(gfx::Rect(size_), root_window_->GetDipScale(),
                                 GenerateLocalSurfaceId());
  OnUpdateOverlayTransform();
  if (needs_animate_) {
    host_->SetNeedsAnimate();
  }
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
    pending_frames_ = 0;

    // Notify CompositorDependenciesAndroid of visibility changes last, to
    // ensure that we don't disable the GPU watchdog until sync IPCs above are
    // completed.
    CompositorDependenciesAndroid::Get().OnCompositorHidden(this);
  } else {
    DCHECK(!host_->IsVisible());
    CompositorDependenciesAndroid::Get().OnCompositorVisible(this);
    RegisterRootFrameSink();
    host_->SetVisible(true);
    has_submitted_frame_since_became_visible_ = false;
    if (layer_tree_frame_sink_request_pending_) {
      HandlePendingLayerTreeFrameSinkRequest();
    }
  }
}

void CompositorImpl::TearDownDisplayAndUnregisterRootFrameSink() {
  // Make a best effort to try to complete pending readbacks.
  // TODO(crbug.com/637035): Consider doing this in a better way,
  // ideally with the guarantee of readbacks completing.
  if (display_private_ && pending_readbacks_) {
    // Note that while this is not a Sync IPC, the call to
    // InvalidateFrameSinkId below will end up triggering a sync call to
    // FrameSinkManager::DestroyCompositorFrameSink, as this is the root
    // frame sink. Because |display_private_| is an associated remote to
    // FrameSinkManager, this subsequent sync call will ensure ordered
    // execution of this call.
    display_private_->ForceImmediateDrawAndSwapIfPossible();
  }
  // Reset |display_private_| first since InvalidateFrameSinkId() will send a
  // sync IPC. This guards against reentrant code using |display_private_|
  // before it can be reset.
  display_private_.reset();
  GetHostFrameSinkManager()->InvalidateFrameSinkId(frame_sink_id_);
  if (display_client_) {
    display_client_->SetPreferredRefreshRate(0);
  }
  display_client_.reset();
  host_begin_frame_observer_.reset();
}

void CompositorImpl::RegisterRootFrameSink() {
  GetHostFrameSinkManager()->RegisterFrameSinkId(
      frame_sink_id_, this, viz::ReportFirstSurfaceActivation::kNo);
  GetHostFrameSinkManager()->SetFrameSinkDebugLabel(frame_sink_id_,
                                                    "CompositorImpl");
  for (auto& frame_sink_id : pending_child_frame_sink_ids_) {
    AddChildFrameSink(frame_sink_id);
  }
  pending_child_frame_sink_ids_.clear();
}

void CompositorImpl::SetWindowBounds(const gfx::Size& size) {
  if (size_ == size) {
    return;
  }

  size_ = size;
  if (host_) {
    // TODO(ccameron): Ensure a valid LocalSurfaceId here.
    host_->SetViewportRectAndScale(gfx::Rect(size_),
                                   root_window_->GetDipScale(),
                                   GenerateLocalSurfaceId());
  }

  if (display_private_) {
    display_private_->Resize(size);
  }

  root_window_->GetLayer()->SetBounds(size);
}

const gfx::Size& CompositorImpl::GetWindowBounds() {
  return size_;
}

void CompositorImpl::SetRequiresAlphaChannel(bool flag) {
  requires_alpha_channel_ = flag;
}

void CompositorImpl::SetNeedsComposite() {
  if (!host_->IsVisible()) {
    return;
  }
  TRACE_EVENT0("compositor", "Compositor::SetNeedsComposite");
  host_->SetNeedsAnimate();
}

void CompositorImpl::MaybeCompositeNow() {
  host_->MaybeCompositeNow();
}

void CompositorImpl::SetNeedsRedraw() {
  host_->SetNeedsRedraw();
}

void CompositorImpl::BeginFrame(const viz::BeginFrameArgs& args) {
  client_->UpdateLayerTreeHost();
  if (needs_animate_) {
    needs_animate_ = false;
    root_window_->Animate(args.frame_time);
  }
}

void CompositorImpl::RequestNewFrameSink() {
  DCHECK(!layer_tree_frame_sink_request_pending_)
      << "LayerTreeFrameSink request is already pending?";

  layer_tree_frame_sink_request_pending_ = true;
  HandlePendingLayerTreeFrameSinkRequest();
}

void CompositorImpl::DidInitializeLayerTreeFrameSink() {
  layer_tree_frame_sink_request_pending_ = false;
}

void CompositorImpl::DidFailToInitializeLayerTreeFrameSink() {
  layer_tree_frame_sink_request_pending_ = false;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&CompositorImpl::RequestNewFrameSink,
                                weak_factory_.GetWeakPtr()));
}

void CompositorImpl::HandlePendingLayerTreeFrameSinkRequest() {
  DCHECK(layer_tree_frame_sink_request_pending_);

  // We might have been made invisible now.
  if (!host_->IsVisible()) {
    return;
  }

  DCHECK(surface_handle_ != gpu::kNullSurfaceHandle);
  BrowserGpuChannelHostFactory::instance()->EstablishGpuChannel(base::BindOnce(
      &CompositorImpl::OnGpuChannelEstablished, weak_factory_.GetWeakPtr()));
}

void CompositorImpl::OnGpuChannelEstablished(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host) {
  // At this point we know we have a valid GPU process, establish our viz
  // connection if needed.
  CompositorDependenciesAndroid::Get().TryEstablishVizConnectionIfNeeded();

  // We might end up queing multiple GpuChannel requests for the same
  // LayerTreeFrameSink request as the visibility of the compositor changes, so
  // the LayerTreeFrameSink request could have been handled already.
  if (!layer_tree_frame_sink_request_pending_) {
    return;
  }

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
      BrowserGpuChannelHostFactory::instance();

  int32_t stream_id = kGpuStreamIdDefault;
  gpu::SchedulingPriority stream_priority = kGpuStreamPriorityUI;

  constexpr bool support_locking = false;
  constexpr bool automatic_flushes = false;
  constexpr bool support_grcontext = true;
  display_color_spaces_ = display::Screen::GetScreen()
                              ->GetDisplayNearestWindow(root_window_)
                              .color_spaces();

  auto context_provider =
      base::MakeRefCounted<viz::ContextProviderCommandBuffer>(
          std::move(gpu_channel_host), factory->GetGpuMemoryBufferManager(),
          stream_id, stream_priority, gpu::kNullSurfaceHandle,
          GURL(std::string("chrome://gpu/CompositorImpl::") +
               std::string("CompositorContextProvider")),
          automatic_flushes, support_locking, support_grcontext,
          GetCompositorContextSharedMemoryLimits(root_window_),
          GetCompositorContextAttributes(
              display_color_spaces_.GetRasterColorSpace(),
              requires_alpha_channel_),
          viz::command_buffer_metrics::ContextType::BROWSER_COMPOSITOR);
  auto result = context_provider->BindToCurrentSequence();

  if (result == gpu::ContextResult::kFatalFailure) {
    LOG(FATAL) << "Fatal failure in creating offscreen context";
  }

  if (result != gpu::ContextResult::kSuccess) {
    HandlePendingLayerTreeFrameSinkRequest();
    return;
  }

  InitializeVizLayerTreeFrameSink(std::move(context_provider));
}

void CompositorImpl::DidSwapBuffers(const gfx::Size& swap_size) {
  client_->DidSwapBuffers(swap_size);

  if (swap_completed_with_size_for_testing_) {
    swap_completed_with_size_for_testing_.Run(swap_size);
  }
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

std::unique_ptr<ui::CompositorLock> CompositorImpl::GetCompositorLock(
    base::TimeDelta timeout) {
  if (!host_) {
    return nullptr;
  }
  return lock_manager_.GetCompositorLock(
      /*client=*/nullptr, timeout, host_->DeferBeginFrame());
}

void CompositorImpl::PostRequestSuccessfulPresentationTimeForNextFrame(
    SuccessfulPresentationTimeCallback callback) {
  RequestSuccessfulPresentationTimeForNextFrame(std::move(callback));
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
  client_->DidSwapFrame(0);
}

std::unique_ptr<ui::WindowAndroidCompositor::ReadbackRef>
CompositorImpl::TakeReadbackRef(const viz::SurfaceId& surface_id) {
  DCHECK(surface_id.is_valid());
  ++pending_readbacks_;
  return std::make_unique<ReadbackRefImpl>(weak_factory_.GetWeakPtr(),
                                           surface_id);
}

void CompositorImpl::RequestCopyOfOutputOnRootLayer(
    std::unique_ptr<viz::CopyOutputRequest> request) {
  host_->RequestCopyOfOutput(std::move(request));
}

void CompositorImpl::SetNeedsAnimate() {
  needs_animate_ = true;
  if (!host_->IsVisible()) {
    return;
  }

  TRACE_EVENT0("compositor", "Compositor::SetNeedsAnimate");
  host_->SetNeedsAnimate();
}

viz::FrameSinkId CompositorImpl::GetFrameSinkId() {
  return frame_sink_id_;
}

void CompositorImpl::AddChildFrameSink(const viz::FrameSinkId& frame_sink_id) {
  if (GetHostFrameSinkManager()->IsFrameSinkIdRegistered(frame_sink_id_)) {
    bool result = GetHostFrameSinkManager()->RegisterFrameSinkHierarchy(
        frame_sink_id_, frame_sink_id);
    DCHECK(result);
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
  if (display.id() != display::Screen::GetScreen()
                          ->GetDisplayNearestWindow(root_window_)
                          .id()) {
    return;
  }

  if (changed_metrics & display::DisplayObserver::DisplayMetric::
                            DISPLAY_METRIC_DEVICE_SCALE_FACTOR) {
    // TODO(ccameron): This is transiently incorrect -- |size_| must be
    // recalculated here as well. Is the call in SetWindowBounds sufficient?
    host_->SetViewportRectAndScale(gfx::Rect(size_),
                                   root_window_->GetDipScale(),
                                   GenerateLocalSurfaceId());
  }

  if (changed_metrics &
      display::DisplayObserver::DisplayMetric::DISPLAY_METRIC_ROTATION) {
    OnUpdateOverlayTransform();
  }

  if (changed_metrics &
      display::DisplayObserver::DisplayMetric::DISPLAY_METRIC_COLOR_SPACE) {
    display_color_spaces_ = display.color_spaces();
    if (display_private_) {
      display_private_->SetDisplayColorSpaces(display_color_spaces_);
    }
  }
}

bool CompositorImpl::IsDrawingFirstVisibleFrame() const {
  return !has_submitted_frame_since_became_visible_;
}

void CompositorImpl::SetVSyncPaused(bool paused) {
  if (vsync_paused_ == paused) {
    return;
  }

  vsync_paused_ = paused;
  if (display_private_) {
    display_private_->SetVSyncPaused(paused);
  }
}

void CompositorImpl::OnUpdateRefreshRate(float refresh_rate) {
  if (display_private_) {
    display_private_->UpdateRefreshRate(refresh_rate);
  }
}

void CompositorImpl::OnUpdateSupportedRefreshRates(
    const std::vector<float>& supported_refresh_rates) {
  if (display_private_) {
    display_private_->SetSupportedRefreshRates(supported_refresh_rates);
  }
}

// WindowAndroid can call this callback
// 1. when display rotation is changed
// 2. when display type is changed in fold device(e.g., main->sub, sub->main),
// the hint can be changed because of panel orientation. e.g., In Galaxy fold,
// main lcd has 270 degrees panel orientation, but sub lcd does not have it.
void CompositorImpl::OnUpdateOverlayTransform() {
  gfx::OverlayTransform hint = root_window_->GetOverlayTransform();
  if (host_) {
    host_->set_display_transform_hint(hint);
  }
}

void CompositorImpl::InitializeVizLayerTreeFrameSink(
    scoped_refptr<viz::ContextProviderCommandBuffer> context_provider) {
  DCHECK(root_window_);

  pending_frames_ = 0;
  gpu_capabilities_ = context_provider->ContextCapabilities();
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      content::GetUIThreadTaskRunner({BrowserTaskType::kUserInput});

  auto root_params = viz::mojom::RootCompositorFrameSinkParams::New();

  // Android requires swap size notifications.
  root_params->send_swap_size_notifications = true;

  // Create interfaces for a root CompositorFrameSink.
  mojo::PendingAssociatedRemote<viz::mojom::CompositorFrameSink> sink_remote;
  root_params->compositor_frame_sink =
      sink_remote.InitWithNewEndpointAndPassReceiver();
  mojo::PendingReceiver<viz::mojom::CompositorFrameSinkClient> client_receiver =
      root_params->compositor_frame_sink_client
          .InitWithNewPipeAndPassReceiver();
  display_private_.reset();
  root_params->display_private =
      display_private_.BindNewEndpointAndPassReceiver();
  display_client_ = std::make_unique<AndroidHostDisplayClient>(this);
  root_params->display_client = display_client_->GetBoundRemote(task_runner);

  const auto& display_props =
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_window_);

  viz::RendererSettings renderer_settings;
  renderer_settings.partial_swap_enabled = true;
  renderer_settings.allow_antialiasing = false;
  renderer_settings.highp_threshold_min = 2048;
  renderer_settings.requires_alpha_channel = requires_alpha_channel_;
  renderer_settings.initial_screen_size = display_props.GetSizeInPixel();
  renderer_settings.color_space = display_color_spaces_.GetOutputColorSpace(
      gfx::ContentColorUsage::kHDR, requires_alpha_channel_);

  root_params->frame_sink_id = frame_sink_id_;
  root_params->widget = surface_handle_;
  root_params->gpu_compositing = true;
  root_params->renderer_settings = renderer_settings;
  root_params->refresh_rate = root_window_->GetRefreshRate();

  GetHostFrameSinkManager()->CreateRootCompositorFrameSink(
      std::move(root_params));

  display_private_->SetSwapCompletionCallbackEnabled(
      enable_swap_completion_callbacks_);
  display_private_->SetDisplayVisible(true);
  display_private_->Resize(size_);
  display_private_->SetDisplayColorSpaces(display_color_spaces_);
  display_private_->SetVSyncPaused(vsync_paused_);
  display_private_->SetSupportedRefreshRates(
      root_window_->GetSupportedRefreshRates());
  MaybeUpdateObserveBeginFrame();

  auto frame_sink = cc::slim::FrameSink::Create(
      std::move(sink_remote), std::move(client_receiver),
      std::move(context_provider), std::move(task_runner),
      BrowserGpuChannelHostFactory::instance()->GetGpuMemoryBufferManager(),
      BrowserMainLoop::GetInstance()->GetIOThreadId());
  host_->SetFrameSink(std::move(frame_sink));
}

viz::LocalSurfaceId CompositorImpl::GenerateLocalSurfaceId() {
  local_surface_id_allocator_.GenerateId();
  return local_surface_id_allocator_.GetCurrentLocalSurfaceId();
}

void CompositorImpl::OnContextCreationResult(
    gpu::ContextResult context_result) {
  if (!gpu::IsFatalOrSurfaceFailure(context_result)) {
    num_of_consecutive_surface_failures_ = 0u;
    return;
  }

  OnFatalOrSurfaceContextCreationFailure(context_result);
}

void CompositorImpl::OnFatalOrSurfaceContextCreationFailure(
    gpu::ContextResult context_result) {
  DCHECK(gpu::IsFatalOrSurfaceFailure(context_result));
  LOG_IF(FATAL, context_result == gpu::ContextResult::kFatalFailure)
      << "Fatal error making Gpu context";

  constexpr size_t kMaxConsecutiveSurfaceFailures = 10u;
  if (++num_of_consecutive_surface_failures_ > kMaxConsecutiveSurfaceFailures) {
    FatalSurfaceFailure();
  }

  if (context_result == gpu::ContextResult::kSurfaceFailure) {
    SetSurface(nullptr, false);
    client_->RecreateSurface();
  }
}

void CompositorImpl::OnFirstSurfaceActivation(const viz::SurfaceInfo& info) {
  NOTREACHED();
}

void CompositorImpl::CacheBackBufferForCurrentSurface() {
  if (window_ && display_private_) {
    cached_back_buffer_ =
        std::make_unique<ScopedCachedBackBuffer>(frame_sink_id_);
  }
}

void CompositorImpl::EvictCachedBackBuffer() {
  cached_back_buffer_.reset();
}

void CompositorImpl::PreserveChildSurfaceControls() {
  if (display_private_) {
    display_private_->PreserveChildSurfaceControls();
  }
}

void CompositorImpl::RequestPresentationTimeForNextFrame(
    PresentationTimeCallback callback) {
  if (!host_) {
    return;
  }
  host_->RequestPresentationTimeForNextFrame(std::move(callback));
}

void CompositorImpl::RequestSuccessfulPresentationTimeForNextFrame(
    SuccessfulPresentationTimeCallback callback) {
  if (!host_) {
    return;
  }
  host_->RequestSuccessfulPresentationTimeForNextFrame(std::move(callback));
}

void CompositorImpl::SetDidSwapBuffersCallbackEnabled(bool enable) {
  if (enable_swap_completion_callbacks_ == enable) {
    return;
  }
  enable_swap_completion_callbacks_ = enable;
  if (display_private_) {
    display_private_->SetSwapCompletionCallbackEnabled(
        enable_swap_completion_callbacks_);
  }
}

void CompositorImpl::DecrementPendingReadbacks() {
  DCHECK_GT(pending_readbacks_, 0u);
  --pending_readbacks_;
}

void CompositorImpl::AddSimpleBeginFrameObserver(
    SimpleBeginFrameObserver* obs) {
  DCHECK(obs);
  DCHECK(!base::Contains(simple_begin_frame_observers_, obs));
  simple_begin_frame_observers_.insert(obs);
  MaybeUpdateObserveBeginFrame();
}

void CompositorImpl::RemoveSimpleBeginFrameObserver(
    SimpleBeginFrameObserver* obs) {
  DCHECK(obs);
  DCHECK(base::Contains(simple_begin_frame_observers_, obs));

  simple_begin_frame_observers_.erase(obs);
  MaybeUpdateObserveBeginFrame();
}

void CompositorImpl::MaybeUpdateObserveBeginFrame() {
  if (simple_begin_frame_observers_.empty()) {
    host_begin_frame_observer_.reset();
    return;
  }

  if (host_begin_frame_observer_) {
    return;
  }

  if (!display_private_) {
    return;
  }

  host_begin_frame_observer_ = std::make_unique<HostBeginFrameObserver>(
      simple_begin_frame_observers_,
      content::GetUIThreadTaskRunner({BrowserTaskType::kUserInput}));
  display_private_->SetStandaloneBeginFrameObserver(
      host_begin_frame_observer_->GetBoundRemote());
}

}  // namespace content
