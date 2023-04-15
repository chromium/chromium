// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/popup_menu_helper_ios.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_ios.h"

namespace content {

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

  popup_client_.set_disconnect_handler(base::BindOnce(
      &PopupMenuHelper::CloseMenu, weak_ptr_factory_.GetWeakPtr()));
}

PopupMenuHelper::~PopupMenuHelper() {
  CloseMenu();
}

void PopupMenuHelper::ShowPopupMenu(
    const gfx::Rect& bounds,
    int item_height,
    double item_font_size,
    int selected_item,
    std::vector<blink::mojom::MenuItemPtr> items,
    bool right_aligned,
    bool allow_multiple_selection) {
  // TODO: Implement this method.
  popup_client_->DidCancel();
  delegate_->OnMenuClosed();
}

void PopupMenuHelper::CloseMenu() {
  popup_client_.reset();
}

RenderWidgetHostViewIOS* PopupMenuHelper::GetRenderWidgetHostView() const {
  return static_cast<RenderWidgetHostViewIOS*>(
      render_frame_host_->GetOutermostMainFrameOrEmbedder()->GetView());
}

void PopupMenuHelper::RenderWidgetHostVisibilityChanged(
    RenderWidgetHost* widget_host,
    bool became_visible) {
  if (!became_visible) {
    CloseMenu();
  }
}

void PopupMenuHelper::RenderWidgetHostDestroyed(RenderWidgetHost* widget_host) {
  CHECK(observation_.IsObservingSource(widget_host));
  observation_.Reset();
}

}  // namespace content
