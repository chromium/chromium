// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_FLING_SCHEDULER_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_FLING_SCHEDULER_ANDROID_H_

#include "base/memory/raw_ptr.h"
#include "content/browser/renderer_host/compositor_impl_android.h"
#include "content/common/content_export.h"
#include "content/common/input/fling_scheduler_base.h"
#include "ui/android/view_android_observer.h"
#include "ui/android/window_android.h"
#include "ui/android/window_android_observer.h"

namespace content {

class RenderWidgetHostImpl;

class CONTENT_EXPORT FlingSchedulerAndroid
    : public FlingSchedulerBase,
      public ui::ViewAndroidObserver,
      public ui::WindowAndroidObserver,
      public CompositorImpl::SimpleBeginFrameObserver {
 public:
  explicit FlingSchedulerAndroid(RenderWidgetHostImpl* host);

  FlingSchedulerAndroid(const FlingSchedulerAndroid&) = delete;
  FlingSchedulerAndroid& operator=(const FlingSchedulerAndroid&) = delete;

  ~FlingSchedulerAndroid() override;

  // FlingControllerSchedulerClient
  void ScheduleFlingProgress(
      base::WeakPtr<FlingController> fling_controller) override;
  void DidStopFlingingOnBrowser(
      base::WeakPtr<FlingController> fling_controller) override;
  bool NeedsBeginFrameForFlingProgress() override;

  // FlingSchedulerBase
  void ProgressFlingOnBeginFrameIfneeded(base::TimeTicks current_time) override;

 protected:
  raw_ptr<RenderWidgetHostImpl> host_;
  base::WeakPtr<FlingController> fling_controller_;

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

  // CompositorImpl::SimpleBeginFrameObserver implementation.
  void OnBeginFrame(base::TimeTicks frame_begin_time) override;

  raw_ptr<ui::ViewAndroid> observed_view_ = nullptr;
  raw_ptr<ui::WindowAndroid> observed_window_ = nullptr;
  raw_ptr<CompositorImpl> observed_compositor_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_FLING_SCHEDULER_ANDROID_H_
