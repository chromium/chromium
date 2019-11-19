// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/fling_scheduler_android.h"

#include "build/build_config.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "ui/compositor/compositor.h"

namespace content {

FlingSchedulerAndroid::FlingSchedulerAndroid(RenderWidgetHostImpl* host)
    : host_(host) {
  DCHECK(host);
}

FlingSchedulerAndroid::~FlingSchedulerAndroid() {
  if (observed_window_)
    observed_window_->RemoveObserver(this);
}

void FlingSchedulerAndroid::ScheduleFlingProgress(
    base::WeakPtr<FlingController> fling_controller) {
  DCHECK(fling_controller);
  fling_controller_ = fling_controller;
  if (!observed_window_) {
    ui::WindowAndroid* window = GetRootWindow();
    // If the root window does not have a Compositor (happens on Android
    // WebView), we'll never receive an OnAnimate call. In this case fall back
    // to BeginFrames coming from the host.
    if (!window || !window->GetCompositor()) {
      host_->SetNeedsBeginFrameForFlingProgress();
      return;
    }
    window->AddObserver(this);
    observed_window_ = window;
  }
  observed_window_->SetNeedsAnimate();
}

void FlingSchedulerAndroid::DidStopFlingingOnBrowser(
    base::WeakPtr<FlingController> fling_controller) {
  DCHECK(fling_controller);
  if (observed_window_) {
    observed_window_->RemoveObserver(this);
    observed_window_ = nullptr;
  }
  fling_controller_ = nullptr;
  host_->DidStopFlinging();
}

bool FlingSchedulerAndroid::NeedsBeginFrameForFlingProgress() {
  ui::WindowAndroid* window = GetRootWindow();
  // If the root window does not have a Compositor (happens on Android
  // WebView), we'll never receive an OnAnimate call. In this case fall back
  // to BeginFrames coming from the host.
  return !window || !window->GetCompositor();
}

void FlingSchedulerAndroid::ProgressFlingOnBeginFrameIfneeded(
    base::TimeTicks current_time) {
  // If a WindowAndroid is being observed, there is no need for BeginFrames
  // coming from the host.
  if (observed_window_)
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

void FlingSchedulerAndroid::OnDetachCompositor() {
  // Once the window's compositor has detached, we will no longer receive
  // OnAnimate calls. Stop observing the window.
  observed_window_->RemoveObserver(this);
  observed_window_ = nullptr;
}

void FlingSchedulerAndroid::OnAnimate(base::TimeTicks frame_begin_time) {
  DCHECK(observed_window_);
  if (fling_controller_)
    fling_controller_->ProgressFling(frame_begin_time);
}

}  // namespace content
