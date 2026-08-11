// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/unbounded_surface_window_android.h"

#include <cmath>

#include "base/android/jni_android.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "cc/slim/frame_sink.h"
#include "cc/slim/layer.h"
#include "cc/slim/layer_tree.h"
#include "cc/slim/surface_layer.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/host/host_display_client.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/browser/renderer_host/compositor_dependencies_android.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/public/android/content_jni_headers/UnboundedSurfacePopupWindow_jni.h"
#include "content/public/browser/browser_thread.h"
#include "gpu/ipc/common/gpu_surface_tracker.h"
#include "services/viz/privileged/mojom/compositing/display_private.mojom.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_manager.mojom.h"
#include "ui/android/window_android.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/gl/android/scoped_java_surface_control.h"

namespace content {

// static
std::unique_ptr<UnboundedSurfaceWindowAndroid>
UnboundedSurfaceWindowAndroid::Create(
    RenderWidgetHostViewAndroid* parent_view,
    mojo::PendingAssociatedReceiver<blink::mojom::UnboundedSurfaceHost> host,
    mojo::PendingAssociatedRemote<blink::mojom::UnboundedSurfaceClient> client,
    const gfx::Rect& bounds_in_dips,
    base::WeakPtr<RenderWidgetHostViewBase> subframe_view) {
  auto window = base::WrapUnique(new UnboundedSurfaceWindowAndroid(
      parent_view, std::move(host), std::move(client),
      std::move(subframe_view)));
  if (!window->InitWindow(bounds_in_dips)) {
    return nullptr;
  }
  return window;
}

UnboundedSurfaceWindowAndroid::UnboundedSurfaceWindowAndroid(
    RenderWidgetHostViewAndroid* parent_view,
    mojo::PendingAssociatedReceiver<blink::mojom::UnboundedSurfaceHost> host,
    mojo::PendingAssociatedRemote<blink::mojom::UnboundedSurfaceClient> client,
    base::WeakPtr<RenderWidgetHostViewBase> subframe_view)
    : HostDisplayClient(gfx::kNullAcceleratedWidget),
      parent_view_(parent_view ? parent_view->GetWeakPtrAndroid() : nullptr),
      subframe_view_(std::move(subframe_view)) {
  if (host.is_valid() && client.is_valid()) {
    receiver_.Bind(std::move(host));
    receiver_.set_disconnect_handler(
        base::BindOnce(&UnboundedSurfaceWindowAndroid::OnConnectionError,
                       base::Unretained(this)));
    client_remote_.Bind(std::move(client));
  }
}

UnboundedSurfaceWindowAndroid::~UnboundedSurfaceWindowAndroid() {
  parent_view_ = nullptr;
  Dismiss();
  display_private_.reset();
  if (root_frame_sink_id_.is_valid() && client_frame_sink_id_.is_valid()) {
    GetHostFrameSinkManager()->UnregisterFrameSinkHierarchy(
        root_frame_sink_id_, client_frame_sink_id_);
  }
  if (client_frame_sink_id_.is_valid()) {
    GetHostFrameSinkManager()->InvalidateFrameSinkId(client_frame_sink_id_,
                                                     this, {});
  }
  if (root_frame_sink_id_.is_valid()) {
    GetHostFrameSinkManager()->InvalidateFrameSinkId(
        root_frame_sink_id_, this,
        base::BindOnce(
            [](gpu::SurfaceHandle surface_handle) {
              if (surface_handle != gpu::kNullSurfaceHandle) {
                gpu::GpuSurfaceTracker::Get()->RemoveSurface(surface_handle);
              }
            },
            surface_handle_));
  }
}

base::WeakPtr<UnboundedSurfaceWindow>
UnboundedSurfaceWindowAndroid::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool UnboundedSurfaceWindowAndroid::is_valid() const {
  return !j_popup_window_.is_null();
}

gfx::NativeWindow UnboundedSurfaceWindowAndroid::GetNativeWindow() const {
  return window_android_;
}

void UnboundedSurfaceWindowAndroid::SetBounds(const gfx::Rect& bounds_in_dips) {
  if (!is_valid() || !parent_view_) {
    return;
  }
  bool size_changed = bounds_in_dips_.size() != bounds_in_dips.size();
  bounds_in_dips_ = bounds_in_dips;
  JNIEnv* env = base::android::AttachCurrentThread();

  float dsf = parent_view_->GetDeviceScaleFactor();
  gfx::Point origin_pixels =
      gfx::ScaleToRoundedPoint(bounds_in_dips.origin(), dsf);
  gfx::Size size_pixels = gfx::ScaleToRoundedSize(bounds_in_dips.size(), dsf);

  Java_UnboundedSurfacePopupWindow_resize(env, j_popup_window_,
                                          origin_pixels.x(), origin_pixels.y());

  if (!size_changed) {
    return;
  }

  root_local_surface_id_allocator_.GenerateId();
  client_local_surface_id_allocator_.GenerateId();

  if (display_private_) {
    display_private_->Resize(size_pixels);
  }
  if (layer_tree_) {
    layer_tree_->SetViewportRectAndScale(
        gfx::Rect(size_pixels), dsf,
        root_local_surface_id_allocator_.GetCurrentLocalSurfaceId());
  }
  if (surface_layer_) {
    surface_layer_->SetBounds(size_pixels);
    surface_layer_->SetSurfaceId(
        viz::SurfaceId(client_frame_sink_id_, GetLocalSurfaceId()),
        cc::DeadlinePolicy::UseDefaultDeadline());
  }

  if (client_remote_.is_bound()) {
    client_remote_->OnSurfaceAllocated(GetFrameSinkId(), GetLocalSurfaceId());
  }
}

viz::FrameSinkId UnboundedSurfaceWindowAndroid::GetFrameSinkId() const {
  return client_frame_sink_id_;
}

viz::LocalSurfaceId UnboundedSurfaceWindowAndroid::GetLocalSurfaceId() const {
  return client_local_surface_id_allocator_.GetCurrentLocalSurfaceId();
}

void UnboundedSurfaceWindowAndroid::GetCompositorFrameSink(
    mojo::PendingReceiver<viz::mojom::CompositorFrameSink> sink,
    mojo::PendingRemote<viz::mojom::CompositorFrameSinkClient> client) {
  GetHostFrameSinkManager()->CreateCompositorFrameSink(
      client_frame_sink_id_, std::move(sink), std::move(client),
      /*render_input_router_config=*/nullptr);
}

void UnboundedSurfaceWindowAndroid::RouteMouseEvent(
    const blink::WebMouseEvent& event) {
  NOTREACHED()
      << "Mouse events are not routed through UnboundedSurfaceWindow on "
         "Android.";
}

gfx::Rect UnboundedSurfaceWindowAndroid::GetBounds() const {
  return bounds_in_dips_;
}

void UnboundedSurfaceWindowAndroid::CopyFromSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    base::TimeDelta timeout,
    base::OnceCallback<void(const content::CopyFromSurfaceResult&)> callback) {
  if (!surface_layer_) {
    std::move(callback).Run(content::CopyFromSurfaceResult());
    return;
  }
  auto request = std::make_unique<viz::CopyOutputRequest>(
      viz::CopyOutputRequest::ResultFormat::RGBA,
      viz::CopyOutputRequest::ResultDestination::kSystemMemory,
      base::BindOnce(
          [](base::OnceCallback<void(const content::CopyFromSurfaceResult&)>
                 callback,
             std::unique_ptr<viz::CopyOutputResult> result) {
            std::move(callback).Run(
                ToCopyFromSurfaceResult(result->ScopedAccessSkBitmap()
                                            .GetOutScopedBitmapAndMetadata()));
          },
          std::move(callback)));
  request->set_result_task_runner(
      base::SingleThreadTaskRunner::GetCurrentDefault());

  viz::SurfaceId surface_id(client_frame_sink_id_, GetLocalSurfaceId());
  GetHostFrameSinkManager()->RequestCopyOfOutput(
      surface_id, std::move(request), /*capture_exact_surface_id=*/false,
      timeout);
}

void UnboundedSurfaceWindowAndroid::EnsureSurfaceSynchronizedForWebTest() {
  if (surface_layer_) {
    surface_layer_->SetSurfaceId(
        viz::SurfaceId(client_frame_sink_id_, GetLocalSurfaceId()),
        cc::DeadlinePolicy::UseInfiniteDeadline());
  }
}

void UnboundedSurfaceWindowAndroid::DidFailToInitializeLayerTreeFrameSink() {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&UnboundedSurfaceWindowAndroid::Dismiss,
                                weak_ptr_factory_.GetWeakPtr()));
}

void UnboundedSurfaceWindowAndroid::DidLoseLayerTreeFrameSink() {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&UnboundedSurfaceWindowAndroid::Dismiss,
                                weak_ptr_factory_.GetWeakPtr()));
}

void UnboundedSurfaceWindowAndroid::Dismiss() {
  if (!is_valid()) {
    return;
  }
  display_private_.reset();
  layer_tree_.reset();
  surface_layer_ = nullptr;
  window_android_ = nullptr;
  if (client_remote_.is_bound()) {
    client_remote_->OnDismissed();
    client_remote_.reset();
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_UnboundedSurfacePopupWindow_dismissPopup(env, j_popup_window_);
  j_popup_window_.Reset();
  if (parent_view_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&RenderWidgetHostViewBase::DestroyUnboundedSurface,
                       parent_view_, GetWeakPtr()));
  }
}

void UnboundedSurfaceWindowAndroid::UpdateBounds(const gfx::Rect& bounds) {
  if (!parent_view_) {
    return;
  }
  parent_view_->UpdateUnboundedSurfaceBoundsInSubframeContext(
      bounds, subframe_view_.get());
}

bool UnboundedSurfaceWindowAndroid::InitWindow(
    const gfx::Rect& bounds_in_dips) {
  if (!parent_view_ || !parent_view_->GetNativeView()) {
    return false;
  }
  ui::WindowAndroid* parent_window_android =
      parent_view_->GetNativeView()->GetWindowAndroid();
  if (!parent_window_android) {
    return false;
  }

  root_frame_sink_id_ = AllocateFrameSinkId();
  GetHostFrameSinkManager()->RegisterFrameSinkId(
      root_frame_sink_id_, this, viz::ReportFirstSurfaceActivation::kNo);
  GetHostFrameSinkManager()->SetFrameSinkDebugLabel(
      root_frame_sink_id_, "UnboundedSurfaceWindowRoot");

  client_frame_sink_id_ = AllocateFrameSinkId();
  GetHostFrameSinkManager()->RegisterFrameSinkId(
      client_frame_sink_id_, this, viz::ReportFirstSurfaceActivation::kNo);
  GetHostFrameSinkManager()->SetFrameSinkDebugLabel(
      client_frame_sink_id_, "UnboundedSurfaceWindowClient");

  GetHostFrameSinkManager()->RegisterFrameSinkHierarchy(root_frame_sink_id_,
                                                        client_frame_sink_id_);

  JNIEnv* env = base::android::AttachCurrentThread();

  bounds_in_dips_ = bounds_in_dips;
  float dsf = parent_view_->GetDeviceScaleFactor();
  gfx::Point origin_pixels =
      gfx::ScaleToRoundedPoint(bounds_in_dips.origin(), dsf);
  gfx::Size size_pixels = gfx::ScaleToRoundedSize(bounds_in_dips.size(), dsf);

  base::android::ScopedJavaLocalRef<jobject> java_window =
      Java_UnboundedSurfacePopupWindow_create(
          env, parent_window_android->GetJavaObject(),
          parent_view_->GetNativeView()->GetContainerView(), origin_pixels.x(),
          origin_pixels.y());

  if (java_window.is_null()) {
    return false;
  }
  j_popup_window_ = base::android::ScopedJavaGlobalRef<jobject>(java_window);

  base::android::ScopedJavaLocalRef<jobject> j_window_android =
      Java_UnboundedSurfacePopupWindow_getWindowAndroid(env, j_popup_window_);
  window_android_ = ui::WindowAndroid::FromJavaWindowAndroid(j_window_android);
  if (!window_android_) {
    return false;
  }

  base::android::ScopedJavaLocalRef<jobject> j_surface_control =
      Java_UnboundedSurfacePopupWindow_getSurfaceControl(env, j_popup_window_);
  if (j_surface_control.is_null()) {
    return false;
  }

  bool release_on_destroy = true;
  gl::ScopedJavaSurfaceControl scoped_java_surface_control(j_surface_control,
                                                           release_on_destroy);
  gpu::GpuSurfaceTracker* tracker = gpu::GpuSurfaceTracker::Get();
  surface_handle_ = tracker->AddSurfaceForNativeWidget(
      gpu::SurfaceRecord(std::move(scoped_java_surface_control)));

  root_local_surface_id_allocator_.GenerateId();
  client_local_surface_id_allocator_.GenerateId();

  layer_tree_ = cc::slim::LayerTree::Create(this);
  layer_tree_->set_background_color(SkColors::kTransparent);
  layer_tree_->SetViewportRectAndScale(
      gfx::Rect(size_pixels), dsf,
      root_local_surface_id_allocator_.GetCurrentLocalSurfaceId());

  CreateDisplayAndFrameSink(size_pixels);

  surface_layer_ = cc::slim::SurfaceLayer::Create();
  surface_layer_->SetIsDrawable(true);
  surface_layer_->SetBackgroundColor(SkColors::kTransparent);
  surface_layer_->SetBounds(size_pixels);
  surface_layer_->SetSurfaceId(
      viz::SurfaceId(client_frame_sink_id_, GetLocalSurfaceId()),
      cc::DeadlinePolicy::UseDefaultDeadline());

  layer_tree_->SetRoot(surface_layer_);
  layer_tree_->SetVisible(true);

  if (client_remote_.is_bound()) {
    client_remote_->OnSurfaceAllocated(GetFrameSinkId(), GetLocalSurfaceId());
  }

  return true;
}

void UnboundedSurfaceWindowAndroid::CreateDisplayAndFrameSink(
    const gfx::Size& surface_size) {
  if (!window_android_) {
    return;
  }

  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host =
      BrowserGpuChannelHostFactory::instance()->GetGpuChannel();
  if (!gpu_channel_host) {
    return;
  }

  CompositorDependenciesAndroid::Get().TryEstablishVizConnectionIfNeeded();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      GetUIThreadTaskRunner({BrowserTaskType::kUserInput});

  auto root_params = viz::mojom::RootCompositorFrameSinkParams::New();

  mojo::PendingAssociatedRemote<viz::mojom::CompositorFrameSink> sink_remote;
  root_params->compositor_frame_sink =
      sink_remote.InitWithNewEndpointAndPassReceiver();
  mojo::PendingReceiver<viz::mojom::CompositorFrameSinkClient> client_receiver =
      root_params->compositor_frame_sink_client
          .InitWithNewPipeAndPassReceiver();
  display_private_.reset();
  root_params->display_private =
      display_private_.BindNewEndpointAndPassReceiver();

  root_params->display_client = GetBoundRemote(task_runner);

  gfx::DisplayColorSpaces display_color_spaces =
      display::Screen::Get()
          ->GetDisplayNearestWindow(window_android_)
          .GetColorSpaces();

  viz::RendererSettings renderer_settings;
  renderer_settings.partial_swap_enabled = true;
  renderer_settings.allow_antialiasing = false;
  renderer_settings.highp_threshold_min = 2048;
  renderer_settings.requires_alpha_channel = true;

  root_params->frame_sink_id = root_frame_sink_id_;
  root_params->widget = surface_handle_;
  root_params->gpu_compositing = true;
  root_params->renderer_settings = renderer_settings;
  root_params->refresh_rate = window_android_->GetRefreshRate();

  GetHostFrameSinkManager()->CreateRootCompositorFrameSink(
      std::move(root_params), /*maybe_wait_on_destruction=*/false);

  display_private_->SetDisplayVisible(true);
  display_private_->Resize(surface_size);
  display_private_->SetDisplayColorSpaces(display_color_spaces);
  display_private_->SetSupportedRefreshRates(
      window_android_->GetSupportedRefreshRates());

  layer_tree_->SetFrameSink(cc::slim::FrameSink::Create(
      std::move(sink_remote), std::move(client_receiver), nullptr,
      GetUIThreadTaskRunner({BrowserTaskType::kUserInput}),
      base::kInvalidThreadId));
  layer_tree_->SetVisible(true);
}

void UnboundedSurfaceWindowAndroid::OnConnectionError() {
  Dismiss();
}

}  // namespace content

DEFINE_JNI(UnboundedSurfacePopupWindow)
