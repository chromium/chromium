// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/fling_scheduler_android.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/public/common/content_features.h"
#include "ui/android/view_android.h"

namespace content {

FlingSchedulerAndroid::FlingSchedulerAndroid(RenderWidgetHostImpl* host)
    : host_(host) {
  DCHECK(host);
}

FlingSchedulerAndroid::~FlingSchedulerAndroid() {
  RemoveCompositorTick();
}

void FlingSchedulerAndroid::ScheduleFlingProgress(
    base::WeakPtr<input::FlingController> fling_controller) {
  DCHECK(fling_controller);
  fling_controller_ = fling_controller;
  if (observed_compositor_)
    return;

  ui::WindowAndroid* window = GetRootWindow();
  if (!window)
    return;

  // If the root window does not have a Compositor (happens on Android
  // WebView), we'll never receive an OnAnimate call. In this case fall back
  // to BeginFrames coming from the host.
  if (!window->GetCompositor()) {
    auto* view = host_->GetView();
    if (view && !view->IsRenderWidgetHostViewChildFrame()) {
      static_cast<RenderWidgetHostViewAndroid*>(view)
          ->SetNeedsBeginFrameForFlingProgress();
    }
    return;
  }

  RequestCompositorTick();
}

void FlingSchedulerAndroid::DidStopFlingingOnBrowser(
    base::WeakPtr<input::FlingController> fling_controller) {
  DCHECK(fling_controller);
  RemoveCompositorTick();
  fling_controller_ = nullptr;
  host_->GetRenderInputRouter()->DidStopFlinging();
}

bool FlingSchedulerAndroid::NeedsBeginFrameForFlingProgress() {
  ui::WindowAndroid* window = GetRootWindow();
  // If the root window does not have a Compositor (happens on Android
  // WebView), we'll never receive an OnAnimate call. In this case fall back
  // to BeginFrames coming from the host.
  return !window || !window->GetCompositor();
}

bool FlingSchedulerAndroid::ShouldUseMobileFlingCurve() {
  return true;
}
gfx::Vector2dF FlingSchedulerAndroid::GetPixelsPerInch(
    const gfx::PointF& position_in_screen) {
  return gfx::Vector2dF(input::kDefaultPixelsPerInch,
                        input::kDefaultPixelsPerInch);
}

void FlingSchedulerAndroid::ProgressFlingOnBeginFrameIfneeded(
    base::TimeTicks current_time) {
  // If a WindowAndroid is being observed, there is no need for BeginFrames
  // coming from the host.
  if (observed_compositor_)
    return;
  if (!fling_controller_)
    return;
  fling_controller_->ProgressFling(current_time);
}

ui::WindowAndroid* FlingSchedulerAndroid::GetRootWindow() {
  if (!host_->GetView() || !host_->GetView()->GetNativeView())
    return nullptr;
  return host_->GetView()->GetNativeView()->GetWindowAndroid();
}

void FlingSchedulerAndroid::RequestCompositorTick() {
  if (!fling_controller_)
    return;

  if (observed_compositor_)
    return;

  if (!observed_view_ && host_->GetView()) {
    if (ui::ViewAndroid* native_view = host_->GetView()->GetNativeView()) {
      native_view->AddObserver(this);
      observed_view_ = native_view;
    }
  }

  ui::WindowAndroid* window = GetRootWindow();
  if (!window)
    return;

  if (!observed_window_) {
    window->AddObserver(this);
    observed_window_ = window;
  }

  CompositorImpl* compositor =
      static_cast<CompositorImpl*>(window->GetCompositor());
  if (!compositor)
    return;

  compositor->AddSimpleBeginFrameObserver(this);
  observed_compositor_ = compositor;
}

void FlingSchedulerAndroid::RemoveCompositorTick() {
  if (observed_view_) {
    observed_view_->RemoveObserver(this);
    observed_view_ = nullptr;
  }

  if (observed_window_) {
    observed_window_->RemoveObserver(this);
    observed_window_ = nullptr;
  }

  if (!observed_compositor_)
    return;
  observed_compositor_->RemoveSimpleBeginFrameObserver(this);
  observed_compositor_ = nullptr;
}

void FlingSchedulerAndroid::OnAttachCompositor() {
  RequestCompositorTick();
}

void FlingSchedulerAndroid::OnDetachCompositor() {
  RemoveCompositorTick();
}

void FlingSchedulerAndroid::OnAttachedToWindow() {
  RequestCompositorTick();
}

void FlingSchedulerAndroid::OnDetachedFromWindow() {
  RemoveCompositorTick();
}

void FlingSchedulerAndroid::OnViewAndroidDestroyed() {
  RemoveCompositorTick();
}

void FlingSchedulerAndroid::OnBeginFrame(base::TimeTicks frame_begin_time,
                                         base::TimeDelta frame_interval) {
  DCHECK(observed_compositor_);
  if (fling_controller_)
    fling_controller_->ProgressFling(frame_begin_time);
}

void FlingSchedulerAndroid::OnBeginFrameSourceShuttingDown() {
  if (observed_compositor_) {
    observed_compositor_->RemoveSimpleBeginFrameObserver(this);
    observed_compositor_ = nullptr;
  }
}

}  // namespace content
