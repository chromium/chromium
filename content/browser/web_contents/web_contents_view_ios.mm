// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/web_contents/web_contents_view_ios.h"

#import <UIKit/UIKit.h>

#include <memory>
#include <string>
#include <utility>

#include "base/mac/scoped_nsobject.h"
#include "content/browser/renderer_host/popup_menu_helper_ios.h"
#include "content/browser/renderer_host/render_widget_host_view_ios.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "ui/base/cocoa/animation_utils.h"

namespace content {

namespace {

WebContentsViewIOS::RenderWidgetHostViewCreateFunction
    g_create_render_widget_host_view = nullptr;

}  // namespace

// static
void WebContentsViewIOS::InstallCreateHookForTests(
    RenderWidgetHostViewCreateFunction create_render_widget_host_view) {
  CHECK_EQ(nullptr, g_create_render_widget_host_view);
  g_create_render_widget_host_view = create_render_widget_host_view;
}

// This class holds a scoped_nsobject so we don't leak that in the header
// of the WebContentsViewIOS.
class WebContentsUIViewHolder {
 public:
  base::scoped_nsobject<UIView> view_;
};

std::unique_ptr<WebContentsView> CreateWebContentsView(
    WebContentsImpl* web_contents,
    std::unique_ptr<WebContentsViewDelegate> delegate,
    RenderViewHostDelegateView** render_view_host_delegate_view) {
  auto rv =
      std::make_unique<WebContentsViewIOS>(web_contents, std::move(delegate));
  *render_view_host_delegate_view = rv.get();
  return rv;
}

WebContentsViewIOS::WebContentsViewIOS(
    WebContentsImpl* web_contents,
    std::unique_ptr<WebContentsViewDelegate> delegate)
    : web_contents_(web_contents) {
  ui_view_ = std::make_unique<WebContentsUIViewHolder>();
  ui_view_->view_ = base::scoped_nsobject<UIView>([[UIView alloc] init]);
  [ui_view_->view_ setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                                       UIViewAutoresizingFlexibleHeight];
}

WebContentsViewIOS::~WebContentsViewIOS() {}

gfx::NativeView WebContentsViewIOS::GetNativeView() const {
  return ui_view_->view_.get();
}

gfx::NativeView WebContentsViewIOS::GetContentNativeView() const {
  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  if (!rwhv) {
    return nullptr;
  }
  return rwhv->GetNativeView();
}

gfx::NativeWindow WebContentsViewIOS::GetTopLevelNativeWindow() const {
  gfx::NativeView view = GetContentNativeView();
  if (!view) {
    return nullptr;
  }
  return gfx::NativeWindow([view window]);
}

gfx::Rect WebContentsViewIOS::GetContainerBounds() const {
  return gfx::Rect();
}

void WebContentsViewIOS::OnCapturerCountChanged() {}

void WebContentsViewIOS::FullscreenStateChanged(bool is_fullscreen) {}

void WebContentsViewIOS::UpdateWindowControlsOverlay(
    const gfx::Rect& bounding_rect) {}

void WebContentsViewIOS::Focus() {}

void WebContentsViewIOS::SetInitialFocus() {}

void WebContentsViewIOS::StoreFocus() {}

void WebContentsViewIOS::RestoreFocus() {}

void WebContentsViewIOS::FocusThroughTabTraversal(bool reverse) {}

DropData* WebContentsViewIOS::GetDropData() const {
  return nullptr;
}

gfx::Rect WebContentsViewIOS::GetViewBounds() const {
  return gfx::Rect();
}

void WebContentsViewIOS::GotFocus(RenderWidgetHostImpl* render_widget_host) {
  web_contents_->NotifyWebContentsFocused(render_widget_host);
}

void WebContentsViewIOS::LostFocus(RenderWidgetHostImpl* render_widget_host) {
  web_contents_->NotifyWebContentsLostFocus(render_widget_host);
}

void WebContentsViewIOS::ShowPopupMenu(
    RenderFrameHost* render_frame_host,
    mojo::PendingRemote<blink::mojom::PopupMenuClient> popup_client,
    const gfx::Rect& bounds,
    int item_height,
    double item_font_size,
    int selected_item,
    std::vector<blink::mojom::MenuItemPtr> menu_items,
    bool right_aligned,
    bool allow_multiple_selection) {
  popup_menu_helper_ = std::make_unique<PopupMenuHelper>(
      this, render_frame_host, std::move(popup_client));
  popup_menu_helper_->ShowPopupMenu(bounds, item_height, item_font_size,
                                    selected_item, std::move(menu_items),
                                    right_aligned, allow_multiple_selection);
}

void WebContentsViewIOS::OnMenuClosed() {
  popup_menu_helper_.reset();
}

void WebContentsViewIOS::CreateView(gfx::NativeView context) {}

RenderWidgetHostViewBase* WebContentsViewIOS::CreateViewForWidget(
    RenderWidgetHost* render_widget_host) {
  if (g_create_render_widget_host_view) {
    return g_create_render_widget_host_view(render_widget_host);
  }
  return new RenderWidgetHostViewIOS(render_widget_host);
}

RenderWidgetHostViewBase* WebContentsViewIOS::CreateViewForChildWidget(
    RenderWidgetHost* render_widget_host) {
  return new RenderWidgetHostViewIOS(render_widget_host);
}

void WebContentsViewIOS::SetPageTitle(const std::u16string& title) {
  // Meaningless on the Mac; widgets don't have a "title" attribute
}

void WebContentsViewIOS::RenderViewReady() {}

void WebContentsViewIOS::RenderViewHostChanged(RenderViewHost* old_host,
                                               RenderViewHost* new_host) {
  ScopedCAActionDisabler disabler;
  if (old_host) {
    auto* rwhv = old_host->GetWidget()->GetView();
    if (rwhv && rwhv->GetNativeView()) {
      static_cast<RenderWidgetHostViewIOS*>(rwhv)->UpdateNativeViewTree(
          nullptr);
    }
  }

  auto* rwhv = new_host->GetWidget()->GetView();
  if (rwhv && rwhv->GetNativeView()) {
    static_cast<RenderWidgetHostViewIOS*>(rwhv)->UpdateNativeViewTree(
        GetNativeView());
  }
}

void WebContentsViewIOS::SetOverscrollControllerEnabled(bool enabled) {}

}  // namespace content
