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
#include "cc/slim/layer.h"
#include "cc/slim/surface_layer.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/public/android/content_jni_headers/UnboundedSurfacePopupWindow_jni.h"
#include "content/public/browser/browser_thread.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_manager.mojom.h"
#include "ui/android/window_android.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/native_ui_types.h"

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
    : parent_view_(parent_view ? parent_view->GetWeakPtrAndroid() : nullptr),
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
  TeardownAndDestroy();
  if (root_frame_sink_id_.is_valid() && client_frame_sink_id_.is_valid()) {
    GetHostFrameSinkManager()->UnregisterFrameSinkHierarchy(
        root_frame_sink_id_, client_frame_sink_id_);
  }
  if (client_frame_sink_id_.is_valid()) {
    GetHostFrameSinkManager()->InvalidateFrameSinkId(client_frame_sink_id_,
                                                     this, {});
  }
}

base::WeakPtr<UnboundedSurfaceWindow>
UnboundedSurfaceWindowAndroid::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool UnboundedSurfaceWindowAndroid::IsValid() const {
  return !j_popup_window_.is_null();
}

gfx::NativeWindow UnboundedSurfaceWindowAndroid::GetNativeWindow() const {
  return window_android_;
}

void UnboundedSurfaceWindowAndroid::SetBounds(const gfx::Rect& bounds_in_dips) {
  if (!IsValid() || !parent_view_) {
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

  if (compositor_) {
    compositor_->Resize(
        size_pixels, dsf,
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

void UnboundedSurfaceWindowAndroid::TeardownAndDestroy() {
  compositor_.reset();
  surface_layer_ = nullptr;
  window_android_ = nullptr;
  if (!j_popup_window_.is_null()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_UnboundedSurfacePopupWindow_dismissPopup(env, j_popup_window_);
    j_popup_window_.Reset();
  }
  if (parent_view_) {
    parent_view_->DestroyUnboundedSurface(GetWeakPtr());
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

  client_frame_sink_id_ = AllocateFrameSinkId();
  GetHostFrameSinkManager()->RegisterFrameSinkId(
      client_frame_sink_id_, this, viz::ReportFirstSurfaceActivation::kNo);
  GetHostFrameSinkManager()->SetFrameSinkDebugLabel(
      client_frame_sink_id_, "UnboundedSurfaceWindowClient");

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

  root_local_surface_id_allocator_.GenerateId();
  client_local_surface_id_allocator_.GenerateId();

  surface_layer_ = cc::slim::SurfaceLayer::Create();
  surface_layer_->SetIsDrawable(true);
  surface_layer_->SetBackgroundColor(SkColors::kTransparent);
  surface_layer_->SetBounds(size_pixels);
  surface_layer_->SetSurfaceId(
      viz::SurfaceId(client_frame_sink_id_, GetLocalSurfaceId()),
      cc::DeadlinePolicy::UseDefaultDeadline());

  compositor_ =
      std::make_unique<AndroidSurfaceControlCompositor>(root_frame_sink_id_);
  if (!compositor_->Initialize(*window_android_, j_surface_control, this,
                               surface_layer_, size_pixels, dsf)) {
    return false;
  }
  GetHostFrameSinkManager()->SetFrameSinkDebugLabel(
      root_frame_sink_id_, "UnboundedSurfaceWindowRoot");
  GetHostFrameSinkManager()->RegisterFrameSinkHierarchy(root_frame_sink_id_,
                                                        client_frame_sink_id_);
  compositor_->Resize(
      size_pixels, dsf,
      root_local_surface_id_allocator_.GetCurrentLocalSurfaceId());

  if (client_remote_.is_bound()) {
    client_remote_->OnSurfaceAllocated(GetFrameSinkId(), GetLocalSurfaceId());
  }

  return true;
}

void UnboundedSurfaceWindowAndroid::OnConnectionError() {
  dismiss_pending_ = true;
  ScheduleDeferredDestroy();
}

}  // namespace content

DEFINE_JNI(UnboundedSurfacePopupWindow)
