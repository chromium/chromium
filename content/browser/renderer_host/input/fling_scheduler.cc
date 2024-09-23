// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/fling_scheduler.h"

#include "build/build_config.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "ui/compositor/compositor.h"
#include "ui/display/screen.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "ui/display/screen.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
#include "ui/display/win/screen_win.h"
#endif  // BUILDFLAG(IS_WIN)

namespace content {

FlingScheduler::FlingScheduler(RenderWidgetHostImpl* host) : host_(host) {
  DCHECK(host);
}

FlingScheduler::~FlingScheduler() {
  if (observed_compositor_)
    observed_compositor_->RemoveAnimationObserver(this);
}

void FlingScheduler::ScheduleFlingProgress(
    base::WeakPtr<input::FlingController> fling_controller) {
  DCHECK(fling_controller);
  fling_controller_ = fling_controller;
  // Don't do anything if a ui::Compositor is already being observed.
  if (observed_compositor_)
    return;

  ui::Compositor* compositor = GetCompositor();
  if (compositor) {
    compositor->AddAnimationObserver(this);
    observed_compositor_ = compositor;
  }
}

void FlingScheduler::DidStopFlingingOnBrowser(
    base::WeakPtr<input::FlingController> fling_controller) {
  DCHECK(fling_controller);
  if (observed_compositor_) {
    observed_compositor_->RemoveAnimationObserver(this);
    observed_compositor_ = nullptr;
  }
  fling_controller_ = nullptr;
  host_->GetRenderInputRouter()->DidStopFlinging();
}

bool FlingScheduler::NeedsBeginFrameForFlingProgress() {
  return !GetCompositor();
}

bool FlingScheduler::ShouldUseMobileFlingCurve() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  return true;
#elif BUILDFLAG(IS_CHROMEOS)
  CHECK(display::Screen::GetScreen());
  return display::Screen::GetScreen()->InTabletMode();
#else
  return false;
#endif
}

gfx::Vector2dF FlingScheduler::GetPixelsPerInch(
    const gfx::PointF& position_in_screen) {
#if BUILDFLAG(IS_WIN)
  return display::win::ScreenWin::GetPixelsPerInch(position_in_screen);
#else
  return gfx::Vector2dF(input::kDefaultPixelsPerInch,
                        input::kDefaultPixelsPerInch);
#endif
}

void FlingScheduler::ProgressFlingOnBeginFrameIfneeded(
    base::TimeTicks current_time) {
  // No fling is active.
  if (!fling_controller_)
    return;

  // FlingProgress will be called within FlingController::OnAnimationStep.
  if (observed_compositor_)
    return;

  fling_controller_->ProgressFling(current_time);
}

ui::Compositor* FlingScheduler::GetCompositor() {
  if (!host_->GetView()) {
    return nullptr;
  }
  return host_->GetView()->GetCompositor();
}

void FlingScheduler::OnAnimationStep(base::TimeTicks timestamp) {
  DCHECK(observed_compositor_);
  if (fling_controller_)
    fling_controller_->ProgressFling(timestamp);
}

void FlingScheduler::OnCompositingShuttingDown(ui::Compositor* compositor) {
  DCHECK_EQ(observed_compositor_, compositor);
  observed_compositor_->RemoveAnimationObserver(this);
  observed_compositor_ = nullptr;
}

}  // namespace content
