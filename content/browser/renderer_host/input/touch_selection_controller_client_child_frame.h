// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_SELECTION_CONTROLLER_CLIENT_CHILD_FRAME_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_SELECTION_CONTROLLER_CLIENT_CHILD_FRAME_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/viz/common/quads/selection.h"
#include "ui/touch_selection/touch_selection_controller.h"
#include "ui/touch_selection/touch_selection_menu_runner.h"

namespace content {
class RenderWidgetHostViewChildFrame;
class TouchSelectionControllerClientManager;

// An implementation of |TouchSelectionControllerClient| to be used by
// implementations of TouchSelectionControllerClientManager. This class serves
// cross-process iframes, which have different renderers than the main frame,
// and thus have their own RenderWidgetHostViewChildFrames. Since a
// TouchSelectionControllerClient is intended to bind these views to the
// TouchSelectionController, we need a different implementation for
// cross-process subframes.
class TouchSelectionControllerClientChildFrame
    : public ui::TouchSelectionControllerClient,
      public ui::TouchSelectionMenuClient {
 public:
  TouchSelectionControllerClientChildFrame(
      RenderWidgetHostViewChildFrame* rwhv,
      TouchSelectionControllerClientManager* manager);

  TouchSelectionControllerClientChildFrame(
      const TouchSelectionControllerClientChildFrame&) = delete;
  TouchSelectionControllerClientChildFrame& operator=(
      const TouchSelectionControllerClientChildFrame&) = delete;

  ~TouchSelectionControllerClientChildFrame() override;

  void DidStopFlinging();
  void OnSwipeToMoveCursorBegin();
  void OnSwipeToMoveCursorEnd();
  void OnHitTestRegionUpdated();
  void UpdateSelectionBoundsIfNeeded(
      const viz::Selection<gfx::SelectionBound>& selection,
      float device_scale_factor);
  void ShowTouchSelectionContextMenu(const gfx::Point& location) override;

 private:
  void TransformSelectionBoundsAndUpdate();

  // ui::TouchSelectionControllerClient:
  bool SupportsAnimation() const override;
  void SetNeedsAnimate() override;
  void MoveCaret(const gfx::PointF& position) override;
  void MoveRangeSelectionExtent(const gfx::PointF& extent) override;
  void SelectBetweenCoordinates(const gfx::PointF& base,
                                const gfx::PointF& extent) override;
  void OnSelectionEvent(ui::SelectionEventType event) override;
  void OnDragUpdate(const ui::TouchSelectionDraggable::Type type,
                    const gfx::PointF& position) override;
  std::unique_ptr<ui::TouchHandleDrawable> CreateDrawable() override;
  void DidScroll() override;

  // ui::TouchSelectionMenuClient:
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  void RunContextMenu() override;
  bool ShouldShowQuickMenu() override;
  std::u16string GetSelectedText() override;

  gfx::Point ConvertFromRoot(const gfx::PointF& point) const;

  // Not owned, non-null for the lifetime of this object.
  raw_ptr<RenderWidgetHostViewChildFrame> rwhv_;
  raw_ptr<TouchSelectionControllerClientManager> manager_;

  // The last selection bounds reported by the view, in view coordinates.
  gfx::SelectionBound selection_start_;
  gfx::SelectionBound selection_end_;
};

}  // namespace content
#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_SELECTION_CONTROLLER_CLIENT_CHILD_FRAME_H_
