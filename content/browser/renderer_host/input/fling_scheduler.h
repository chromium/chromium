// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_FLING_SCHEDULER_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_FLING_SCHEDULER_H_

#include "base/memory/raw_ptr.h"
#include "components/input/fling_controller.h"
#include "components/input/fling_scheduler_base.h"
#include "content/common/content_export.h"
#include "ui/compositor/compositor_animation_observer.h"

namespace ui {
class Compositor;
}

namespace content {

class RenderWidgetHostImpl;

class CONTENT_EXPORT FlingScheduler : public input::FlingSchedulerBase,
                                      private ui::CompositorAnimationObserver {
 public:
  FlingScheduler(RenderWidgetHostImpl* host);

  FlingScheduler(const FlingScheduler&) = delete;
  FlingScheduler& operator=(const FlingScheduler&) = delete;

  ~FlingScheduler() override;

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
  virtual ui::Compositor* GetCompositor();
  raw_ptr<RenderWidgetHostImpl> host_;
  base::WeakPtr<input::FlingController> fling_controller_;
  raw_ptr<ui::Compositor> observed_compositor_ = nullptr;

 private:
  // ui::CompositorAnimationObserver
  void OnAnimationStep(base::TimeTicks timestamp) override;
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_FLING_SCHEDULER_H_
