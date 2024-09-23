// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/web_contents/web_contents_view_ios.h"

#import <UIKit/UIKit.h>

#include <memory>
#include <string>
#include <utility>

#include "base/apple/foundation_util.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/renderer_host/popup_menu_helper_ios.h"
#include "content/browser/renderer_host/render_widget_host_view_ios.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "ui/base/cocoa/animation_utils.h"
#include "ui/gfx/native_widget_types.h"

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

// This class holds strongly so we don't leak that in the header of the
// WebContentsViewIOS.
class WebContentsUIViewHolder {
 public:
  UIScrollView* __strong view_;
};

std::unique_ptr<WebContentsView> CreateWebContentsView(
    WebContentsImpl* web_contents,
    std::unique_ptr<WebContentsViewDelegate> delegate,
    raw_ptr<RenderViewHostDelegateView>* render_view_host_delegate_view) {
  auto rv =
      std::make_unique<WebContentsViewIOS>(web_contents, std::move(delegate));
  *render_view_host_delegate_view = rv.get();
  return rv;
}

WebContentsViewIOS::WebContentsViewIOS(
    WebContentsImpl* web_contents,
    std::unique_ptr<WebContentsViewDelegate> delegate)
    : web_contents_(web_contents), delegate_(std::move(delegate)) {
  ui_view_ = std::make_unique<WebContentsUIViewHolder>();
  ui_view_->view_ = [[UIScrollView alloc] init];
  [ui_view_->view_ setScrollEnabled:NO];
  [ui_view_->view_ setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                                       UIViewAutoresizingFlexibleHeight];
}

WebContentsViewIOS::~WebContentsViewIOS() {}

gfx::NativeView WebContentsViewIOS::GetNativeView() const {
  return gfx::NativeView(ui_view_->view_);
}

gfx::NativeView WebContentsViewIOS::GetContentNativeView() const {
  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  if (!rwhv) {
    return gfx::NativeView();
  }
  return rwhv->GetNativeView();
}

gfx::NativeWindow WebContentsViewIOS::GetTopLevelNativeWindow() const {
  gfx::NativeView view = GetContentNativeView();
  if (!view) {
    return gfx::NativeWindow();
  }
  return gfx::NativeWindow(view.Get().window);
}

gfx::Rect WebContentsViewIOS::GetContainerBounds() const {
  return gfx::Rect();
}

void WebContentsViewIOS::OnCapturerCountChanged() {}

void WebContentsViewIOS::FullscreenStateChanged(bool is_fullscreen) {
  if (is_fullscreen && popup_menu_helper_) {
    popup_menu_helper_->CloseMenu();
  }
}

void WebContentsViewIOS::UpdateWindowControlsOverlay(
    const gfx::Rect& bounding_rect) {}

void WebContentsViewIOS::Focus() {
  if (delegate_) {
    delegate_->ResetStoredFocus();
  }

  // Focus the the fullscreen view, if one exists; otherwise, focus the content
  // native view. This ensures that the view currently attached to a NSWindow is
  // being used to query or set first responder state.
  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  if (!rwhv) {
    return;
  }

  static_cast<RenderWidgetHostViewBase*>(rwhv)->Focus();
}

void WebContentsViewIOS::SetInitialFocus() {
  if (delegate_) {
    delegate_->ResetStoredFocus();
  }

  if (web_contents_->FocusLocationBarByDefault()) {
    web_contents_->SetFocusToLocationBar();
  } else {
    Focus();
  }
}

void WebContentsViewIOS::StoreFocus() {
  if (delegate_) {
    delegate_->StoreFocus();
  }
}

void WebContentsViewIOS::RestoreFocus() {
  if (delegate_ && delegate_->RestoreFocus()) {
    return;
  }

  // Fall back to the default focus behavior if we could not restore focus.
  // TODO(shess): If location-bar gets focus by default, this will
  // select-all in the field.  If there was a specific selection in
  // the field when we navigated away from it, we should restore
  // that selection.
  SetInitialFocus();
}

void WebContentsViewIOS::FocusThroughTabTraversal(bool reverse) {
  if (delegate_) {
    delegate_->ResetStoredFocus();
  }

  web_contents_->GetRenderViewHost()->SetInitialFocus(reverse);
}

DropData* WebContentsViewIOS::GetDropData() const {
  return nullptr;
}

gfx::Rect WebContentsViewIOS::GetViewBounds() const {
  return gfx::Rect(ui_view_->view_.contentSize.width,
                   ui_view_->view_.contentSize.height);
}

void WebContentsViewIOS::GotFocus(RenderWidgetHostImpl* render_widget_host) {
  web_contents_->NotifyWebContentsFocused(render_widget_host);
}

void WebContentsViewIOS::LostFocus(RenderWidgetHostImpl* render_widget_host) {
  web_contents_->NotifyWebContentsLostFocus(render_widget_host);
}

void WebContentsViewIOS::ShowContextMenu(RenderFrameHost& render_frame_host,
                                         const ContextMenuParams& params) {
  if (delegate_) {
    delegate_->ShowContextMenu(render_frame_host, params);
  } else {
    DLOG(ERROR) << "Cannot show context menus without a delegate.";
  }
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
          gfx::NativeView());
    }
  }

  auto* rwhv = new_host->GetWidget()->GetView();
  if (rwhv && rwhv->GetNativeView()) {
    static_cast<RenderWidgetHostViewIOS*>(rwhv)->UpdateNativeViewTree(
        GetNativeView());
  }
  web_contents_->UpdateBrowserControlsState(cc::BrowserControlsState::kBoth,
                                            cc::BrowserControlsState::kHidden,
                                            false, std::nullopt);
}

void WebContentsViewIOS::SetOverscrollControllerEnabled(bool enabled) {}

int WebContentsViewIOS::GetTopControlsHeight() const {
  auto* delegate = web_contents_->GetDelegate();
  return delegate ? delegate->GetTopControlsHeight() : 0;
}

int WebContentsViewIOS::GetTopControlsMinHeight() const {
  auto* delegate = web_contents_->GetDelegate();
  return delegate ? delegate->GetTopControlsMinHeight() : 0;
}

int WebContentsViewIOS::GetBottomControlsHeight() const {
  auto* delegate = web_contents_->GetDelegate();
  return delegate ? delegate->GetBottomControlsHeight() : 0;
}

int WebContentsViewIOS::GetBottomControlsMinHeight() const {
  auto* delegate = web_contents_->GetDelegate();
  return delegate ? delegate->GetBottomControlsMinHeight() : 0;
}

bool WebContentsViewIOS::ShouldAnimateBrowserControlsHeightChanges() const {
  auto* delegate = web_contents_->GetDelegate();
  return delegate && delegate->ShouldAnimateBrowserControlsHeightChanges();
}

bool WebContentsViewIOS::DoBrowserControlsShrinkRendererSize() const {
  auto* delegate = web_contents_->GetDelegate();
  return delegate &&
         delegate->DoBrowserControlsShrinkRendererSize(web_contents_);
}

bool WebContentsViewIOS::OnlyExpandTopControlsAtPageTop() const {
  auto* delegate = web_contents_->GetDelegate();
  return delegate && delegate->OnlyExpandTopControlsAtPageTop();
}

BackForwardTransitionAnimationManager*
WebContentsViewIOS::GetBackForwardTransitionAnimationManager() {
  return nullptr;
}

}  // namespace content
