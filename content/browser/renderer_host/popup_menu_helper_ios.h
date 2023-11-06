// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_POPUP_MENU_HELPER_IOS_H_
#define CONTENT_BROWSER_RENDERER_HOST_POPUP_MENU_HELPER_IOS_H_

#include "base/scoped_observation.h"
#include "content/browser/renderer_host/popup_menu_interaction_delegate.h"
#include "content/public/browser/render_widget_host_observer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/choosers/popup_menu.mojom.h"
#include "ui/gfx/geometry/rect.h"

@class WebMenuRunner;

namespace content {

class RenderFrameHost;
class RenderFrameHostImpl;
class RenderWidgetHostViewIOS;

class PopupMenuHelper : public RenderWidgetHostObserver,
                        public MenuInteractionDelegate {
 public:
  class Delegate {
   public:
    virtual void OnMenuClosed() = 0;
  };

  // Creates a PopupMenuHelper that will notify |render_frame_host| when a user
  // selects or cancels the popup. |delegate| is notified when the menu is
  // closed.
  PopupMenuHelper(
      Delegate* delegate,
      RenderFrameHost* render_frame_host,
      mojo::PendingRemote<blink::mojom::PopupMenuClient> popup_client);

  PopupMenuHelper(const PopupMenuHelper&) = delete;
  PopupMenuHelper& operator=(const PopupMenuHelper&) = delete;

  ~PopupMenuHelper() override;
  void CloseMenu();

  // Shows the popup menu and notifies the RenderFrameHost of the selection/
  // cancellation.
  void ShowPopupMenu(const gfx::Rect& bounds,
                     int item_height,
                     double item_font_size,
                     int selected_item,
                     std::vector<blink::mojom::MenuItemPtr> items,
                     bool right_aligned,
                     bool allow_multiple_selection);

  // MenuInteractionDelegate implementation:
  void OnMenuItemSelected(int idx) override;
  void OnMenuCanceled() override;

  // Immediately return from ShowPopupMenu.
  CONTENT_EXPORT static void DontShowPopupMenuForTesting();

 private:
  // RenderWidgetHostObserver implementation:
  void RenderWidgetHostVisibilityChanged(RenderWidgetHost* widget_host,
                                         bool became_visible) override;
  void RenderWidgetHostDestroyed(RenderWidgetHost* widget_host) override;

  RenderWidgetHostViewIOS* GetRenderWidgetHostView() const;

  raw_ptr<Delegate> delegate_;  // Weak. Owns |this|.

  base::ScopedObservation<RenderWidgetHost, RenderWidgetHostObserver>
      observation_{this};
  base::WeakPtr<RenderFrameHostImpl> render_frame_host_;
  mojo::Remote<blink::mojom::PopupMenuClient> popup_client_;

  WebMenuRunner* __strong menu_runner_;

  base::WeakPtrFactory<PopupMenuHelper> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_POPUP_MENU_HELPER_IOS_H_
