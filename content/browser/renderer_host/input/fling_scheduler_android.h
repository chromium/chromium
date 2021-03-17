// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_FLING_SCHEDULER_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_FLING_SCHEDULER_ANDROID_H_

#include "content/browser/renderer_host/input/fling_scheduler_base.h"
#include "content/common/content_export.h"
#include "ui/android/window_android.h"
#include "ui/android/window_android_observer.h"

namespace content {

class RenderWidgetHostImpl;

class CONTENT_EXPORT FlingSchedulerAndroid : public FlingSchedulerBase,
                                             public ui::WindowAndroidObserver {
 public:
  explicit FlingSchedulerAndroid(RenderWidgetHostImpl* host);
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
  RenderWidgetHostImpl* host_;
  base::WeakPtr<FlingController> fling_controller_;

 private:
  ui::WindowAndroid* GetRootWindow();

  // WindowAndroidObserver implementation.
  void OnCompositingDidCommit() override {}
  void OnRootWindowVisibilityChanged(bool visible) override {}
  void OnAttachCompositor() override {}
  void OnDetachCompositor() override;
  void OnAnimate(base::TimeTicks frame_begin_time) override;
  void OnActivityStopped() override {}
  void OnActivityStarted() override {}

  ui::WindowAndroid* observed_window_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FlingSchedulerAndroid);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_FLING_SCHEDULER_ANDROID_H_
