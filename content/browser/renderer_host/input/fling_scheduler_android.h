// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_FLING_SCHEDULER_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_FLING_SCHEDULER_ANDROID_H_

#include "base/memory/raw_ptr.h"
#include "components/input/fling_scheduler_base.h"
#include "content/browser/renderer_host/compositor_impl_android.h"
#include "content/common/content_export.h"
#include "ui/android/view_android_observer.h"
#include "ui/android/window_android.h"
#include "ui/android/window_android_observer.h"
#include "ui/compositor/host_begin_frame_observer.h"

namespace content {

class RenderWidgetHostImpl;

class CONTENT_EXPORT FlingSchedulerAndroid
    : public input::FlingSchedulerBase,
      public ui::ViewAndroidObserver,
      public ui::WindowAndroidObserver,
      public ui::HostBeginFrameObserver::SimpleBeginFrameObserver {
 public:
  explicit FlingSchedulerAndroid(RenderWidgetHostImpl* host);

  FlingSchedulerAndroid(const FlingSchedulerAndroid&) = delete;
  FlingSchedulerAndroid& operator=(const FlingSchedulerAndroid&) = delete;

  ~FlingSchedulerAndroid() override;

  // FlingControllerSchedulerClient
  void ScheduleFlingProgress(
      base::WeakPtr<input::FlingController> fling_controller) override;
  void DidStopFlingingOnBrowser(
      base::WeakPtr<input::FlingController> fling_controller) override;
  bool NeedsBeginFrameForFlingProgress() override;
  bool ShouldUseMobileFlingCurve() override;
  gfx::Vector2dF GetPixelsPerInch(
      const gfx::PointF& position_in_screen) override;

  // FlingSchedulerBase
  void ProgressFlingOnBeginFrameIfneeded(base::TimeTicks current_time) override;

 protected:
  raw_ptr<RenderWidgetHostImpl> host_;
  base::WeakPtr<input::FlingController> fling_controller_;

 private:
  ui::WindowAndroid* GetRootWindow();
  void RequestCompositorTick();
  void RemoveCompositorTick();

  // ui::WindowAndroidObserver implementation.
  void OnRootWindowVisibilityChanged(bool visible) override {}
  void OnAttachCompositor() override;
  void OnDetachCompositor() override;
  void OnAnimate(base::TimeTicks begin_frame_time) override {}
  void OnActivityStopped() override {}
  void OnActivityStarted() override {}

  // ui::ViewAndroidObserver implementation:
  void OnAttachedToWindow() override;
  void OnDetachedFromWindow() override;
  void OnViewAndroidDestroyed() override;

  // ui::HostBeginFrameObserver::SimpleBeginFrameObserver implementation.
  void OnBeginFrame(base::TimeTicks frame_begin_time,
                    base::TimeDelta frame_interval) override;
  void OnBeginFrameSourceShuttingDown() override;

  raw_ptr<ui::ViewAndroid> observed_view_ = nullptr;
  raw_ptr<ui::WindowAndroid> observed_window_ = nullptr;
  raw_ptr<CompositorImpl> observed_compositor_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_FLING_SCHEDULER_ANDROID_H_
