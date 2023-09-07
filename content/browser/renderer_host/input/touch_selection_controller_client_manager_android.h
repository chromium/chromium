// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_SELECTION_CONTROLLER_CLIENT_MANAGER_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_SELECTION_CONTROLLER_CLIENT_MANAGER_ANDROID_H_

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "content/public/browser/touch_selection_controller_client_manager.h"
#include "ui/touch_selection/touch_selection_controller.h"

namespace content {

class RenderWidgetHostViewAndroid;

class TouchSelectionControllerClientManagerAndroid
    : public TouchSelectionControllerClientManager,
      public ui::TouchSelectionControllerClient {
 public:
  explicit TouchSelectionControllerClientManagerAndroid(
      RenderWidgetHostViewAndroid* rwhv);

  TouchSelectionControllerClientManagerAndroid(
      const TouchSelectionControllerClientManagerAndroid&) = delete;
  TouchSelectionControllerClientManagerAndroid& operator=(
      const TouchSelectionControllerClientManagerAndroid&) = delete;

  ~TouchSelectionControllerClientManagerAndroid() override;

  // TouchSelectionControllerClientManager implementation.
  void DidStopFlinging() override;
  void OnSwipeToMoveCursorBegin() override;
  void OnSwipeToMoveCursorEnd() override;
  void OnClientHitTestRegionUpdated(
      ui::TouchSelectionControllerClient* client) override;
  void UpdateClientSelectionBounds(
      const gfx::SelectionBound& start,
      const gfx::SelectionBound& end,
      ui::TouchSelectionControllerClient* client,
      ui::TouchSelectionMenuClient* menu_client) override;
  void InvalidateClient(ui::TouchSelectionControllerClient* client) override;
  ui::TouchSelectionController* GetTouchSelectionController() override;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void ShowContextMenu(const gfx::Point& location) override;

  // TouchSelectionControllerClient implementation.
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
  void ShowTouchSelectionContextMenu(const gfx::Point& location) override;

  bool has_active_selection() const {
    return manager_selection_start_.HasHandle() ||
           manager_selection_end_.HasHandle();
  }

 private:
  // Not owned, assumed to be non-null for the lifetime of this object.
  raw_ptr<RenderWidgetHostViewAndroid> rwhv_;

  raw_ptr<TouchSelectionControllerClient> active_client_;
  gfx::SelectionBound manager_selection_start_;
  gfx::SelectionBound manager_selection_end_;
  base::ObserverList<TouchSelectionControllerClientManager::Observer>
      observers_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_SELECTION_CONTROLLER_CLIENT_MANAGER_ANDROID_H_
