// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/popup_menu_helper_mac.h"

#include "base/numerics/safe_conversions.h"
#import "content/app_shim_remote_cocoa/render_widget_host_view_cocoa.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#include "content/public/browser/web_contents.h"
#import "ui/base/cocoa/base_view.h"

namespace content {

namespace {

bool g_allow_showing_popup_menus = true;

}  // namespace

PopupMenuHelper::PopupMenuHelper(
    Delegate* delegate,
    RenderFrameHost* render_frame_host,
    mojo::PendingRemote<blink::mojom::PopupMenuClient> popup_client)
    : delegate_(delegate),
      render_frame_host_(
          static_cast<RenderFrameHostImpl*>(render_frame_host)->GetWeakPtr()),
      popup_client_(std::move(popup_client)) {
  RenderWidgetHost* widget_host =
      render_frame_host->GetRenderViewHost()->GetWidget();
  observation_.Observe(widget_host);

  popup_client_.set_disconnect_handler(
      base::BindOnce(&PopupMenuHelper::Hide, weak_ptr_factory_.GetWeakPtr()));
}

PopupMenuHelper::~PopupMenuHelper() {
  Hide();
}

void PopupMenuHelper::ShowPopupMenu(
    const gfx::Rect& bounds,
    int item_height,
    double item_font_size,
    int selected_item,
    std::vector<blink::mojom::MenuItemPtr> items,
    bool right_aligned,
    bool allow_multiple_selection) {
  // Only single selection list boxes show a popup on Mac.
  DCHECK(!allow_multiple_selection);
  if (!g_allow_showing_popup_menus)
    return;

  RenderWidgetHostViewMac* rwhvm = GetRenderWidgetHostView();
  auto* web_contents = rwhvm->GetWebContents();

  // Convert element_bounds to be in screen.
  gfx::Rect client_area = web_contents->GetContainerBounds();
  gfx::Rect bounds_in_screen = bounds + client_area.OffsetFromOrigin();

  // The new popup menu would overlap the permission prompt, which could lead to
  // users making decisions based on incorrect information. We should close the
  // popup if it intersects with the permission prompt.
  auto permission_exclusion_area_bounds =
      PermissionControllerImpl::FromBrowserContext(
          web_contents->GetBrowserContext())
          ->GetExclusionAreaBoundsInScreen(web_contents);
  if (permission_exclusion_area_bounds &&
      permission_exclusion_area_bounds->Intersects(bounds_in_screen)) {
    popup_client_->DidCancel();
    delegate_->OnMenuClosed();  // May delete |this|.
    return;
  }

  remote_runner_.reset();
  rwhvm->GetNSView()->DisplayPopupMenu(
      remote_cocoa::mojom::PopupMenu::New(
          std::move(items), bounds, item_font_size, right_aligned,
          selected_item, remote_runner_.BindNewPipeAndPassReceiver()),
      base::BindOnce(&PopupMenuHelper::PopupMenuClosed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PopupMenuHelper::Hide() {
  if (remote_runner_) {
    remote_runner_->Hide();
  }
  popup_was_hidden_ = true;
  popup_client_.reset();
}

// static
void PopupMenuHelper::DontShowPopupMenuForTesting() {
  g_allow_showing_popup_menus = false;
}

RenderWidgetHostViewMac* PopupMenuHelper::GetRenderWidgetHostView() const {
  return static_cast<RenderWidgetHostViewMac*>(
      render_frame_host_->GetOutermostMainFrameOrEmbedder()->GetView());
}

void PopupMenuHelper::RenderWidgetHostVisibilityChanged(
    RenderWidgetHost* widget_host,
    bool became_visible) {
  if (!became_visible)
    Hide();
}

void PopupMenuHelper::RenderWidgetHostDestroyed(RenderWidgetHost* widget_host) {
  DCHECK(observation_.IsObservingSource(widget_host));
  observation_.Reset();
}

void PopupMenuHelper::PopupMenuClosed(std::optional<uint32_t> selected_item) {
  // The RenderFrameHost may be deleted while running the menu, or it may have
  // requested the close. Don't notify in these cases.
  if (popup_client_ && !popup_was_hidden_) {
    if (selected_item.has_value()) {
      popup_client_->DidAcceptIndices(
          {base::saturated_cast<int32_t>(*selected_item)});
    } else {
      popup_client_->DidCancel();
    }
  }

  delegate_->OnMenuClosed();  // May delete |this|.
}

}  // namespace content
