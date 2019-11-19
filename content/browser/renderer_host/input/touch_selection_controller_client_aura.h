// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_SELECTION_CONTROLLER_CLIENT_AURA_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_SELECTION_CONTROLLER_CLIENT_AURA_H_

#include <memory>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "content/public/browser/touch_selection_controller_client_manager.h"
#include "ui/touch_selection/touch_selection_controller.h"
#include "ui/touch_selection/touch_selection_menu_runner.h"

namespace content {
struct ContextMenuParams;
class RenderWidgetHostViewAura;

// An implementation of |TouchSelectionControllerClient| to be used in Aura's
// implementation of touch selection for contents.
class CONTENT_EXPORT TouchSelectionControllerClientAura
    : public ui::TouchSelectionControllerClient,
      public ui::TouchSelectionMenuClient,
      public TouchSelectionControllerClientManager {
 public:
  explicit TouchSelectionControllerClientAura(RenderWidgetHostViewAura* rwhva);
  ~TouchSelectionControllerClientAura() override;

  // Called when |rwhva_|'s window is moved, to update the quick menu's
  // position.
  void OnWindowMoved();

  // Called on first touch down/last touch up to hide/show the quick menu.
  void OnTouchDown();
  void OnTouchUp();

  // Called when touch scroll starts/completes to hide/show touch handles and
  // the quick menu.
  void OnScrollStarted();
  void OnScrollCompleted();

  // Gives an opportunity to the client to handle context menu request and show
  // the quick menu instead, if appropriate. Returns |true| to indicate that no
  // further handling is needed.
  // TODO(mohsen): This is to match Chrome on Android behavior. However, it is
  // better not to send context menu request from the renderer in this case and
  // instead decide in the client about showing the quick menu in response to
  // selection events. (http://crbug.com/548245)
  bool HandleContextMenu(const ContextMenuParams& params);

  void UpdateClientSelectionBounds(const gfx::SelectionBound& start,
                                   const gfx::SelectionBound& end);

  // TouchSelectionControllerClientManager.
  void DidStopFlinging() override;
  void UpdateClientSelectionBounds(
      const gfx::SelectionBound& start,
      const gfx::SelectionBound& end,
      ui::TouchSelectionControllerClient* client,
      ui::TouchSelectionMenuClient* menu_client) override;
  void InvalidateClient(ui::TouchSelectionControllerClient* client) override;
  ui::TouchSelectionController* GetTouchSelectionController() override;
  void AddObserver(
      TouchSelectionControllerClientManager::Observer* observer) override;
  void RemoveObserver(
      TouchSelectionControllerClientManager::Observer* observer) override;

 private:
  friend class TestTouchSelectionControllerClientAura;
  class EnvEventObserver;
  class EnvPreTargetHandler;

  bool IsQuickMenuAvailable() const;
  void ShowQuickMenu();
  void UpdateQuickMenu();

  // ui::TouchSelectionControllerClient:
  bool SupportsAnimation() const override;
  void SetNeedsAnimate() override;
  void MoveCaret(const gfx::PointF& position) override;
  void MoveRangeSelectionExtent(const gfx::PointF& extent) override;
  void SelectBetweenCoordinates(const gfx::PointF& base,
                                const gfx::PointF& extent) override;
  void OnSelectionEvent(ui::SelectionEventType event) override;
  void OnDragUpdate(const gfx::PointF& position) override;
  std::unique_ptr<ui::TouchHandleDrawable> CreateDrawable() override;
  void DidScroll() override;

  // ui::TouchSelectionMenuClient:
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  void RunContextMenu() override;
  bool ShouldShowQuickMenu() override;
  base::string16 GetSelectedText() override;

  // Not owned, non-null for the lifetime of this object.
  RenderWidgetHostViewAura* rwhva_;

  class InternalClient : public TouchSelectionControllerClient {
   public:
    explicit InternalClient(RenderWidgetHostViewAura* rwhva) : rwhva_(rwhva) {}
    ~InternalClient() final {}

    bool SupportsAnimation() const final;
    void SetNeedsAnimate() final;
    void MoveCaret(const gfx::PointF& position) final;
    void MoveRangeSelectionExtent(const gfx::PointF& extent) final;
    void SelectBetweenCoordinates(const gfx::PointF& base,
                                  const gfx::PointF& extent) final;
    void OnSelectionEvent(ui::SelectionEventType event) final;
    void OnDragUpdate(const gfx::PointF& position) final;
    std::unique_ptr<ui::TouchHandleDrawable> CreateDrawable() final;
    void DidScroll() override;

   private:
    RenderWidgetHostViewAura* rwhva_;
  } internal_client_;

  // Keep track of which client interface to use.
  TouchSelectionControllerClient* active_client_;
  TouchSelectionMenuClient* active_menu_client_;
  gfx::SelectionBound manager_selection_start_;
  gfx::SelectionBound manager_selection_end_;

  base::ObserverList<TouchSelectionControllerClientManager::Observer>
      observers_;

  base::RetainingOneShotTimer quick_menu_timer_;
  bool quick_menu_requested_;
  bool touch_down_;
  bool scroll_in_progress_;
  bool handle_drag_in_progress_;

  bool show_quick_menu_immediately_for_test_;

  // An event observer that deactivates touch selection on certain input events.
  std::unique_ptr<EnvEventObserver> env_event_observer_;

  DISALLOW_COPY_AND_ASSIGN(TouchSelectionControllerClientAura);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_SELECTION_CONTROLLER_CLIENT_AURA_H_
