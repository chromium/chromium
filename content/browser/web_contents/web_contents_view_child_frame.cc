// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_view_child_frame.h"

#include <utility>

#include "build/build_config.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/display/display_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(IS_MAC)
#include "content/browser/renderer_host/popup_menu_helper_mac.h"
#endif

using blink::DragOperationsMask;

namespace content {

namespace {
#if BUILDFLAG(IS_MAC)
class NoOpPopupMenuHelperDelegate : public PopupMenuHelper::Delegate {
 public:
  void OnMenuClosed() final {
    // Nothing to clean up, as `PopupMenuHelper` deletes itself at the end of
    // `WebContentsViewChildFrame::ShowPopupMenu`.
  }
};
#endif
}  // namespace

WebContentsViewChildFrame::WebContentsViewChildFrame(
    WebContentsImpl* web_contents,
    std::unique_ptr<WebContentsViewDelegate> delegate,
    raw_ptr<RenderViewHostDelegateView>* delegate_view)
    : web_contents_(web_contents), delegate_(std::move(delegate)) {
  *delegate_view = this;
}

WebContentsViewChildFrame::~WebContentsViewChildFrame() = default;

WebContentsView* WebContentsViewChildFrame::GetOuterView() {
  if (auto* outer_web_contents = web_contents_->GetOuterWebContents()) {
    return outer_web_contents->GetView();
  }

  return nullptr;
}

const WebContentsView* WebContentsViewChildFrame::GetOuterView() const {
  if (auto* outer_web_contents = web_contents_->GetOuterWebContents()) {
    return outer_web_contents->GetView();
  }

  return nullptr;
}

RenderViewHostDelegateView* WebContentsViewChildFrame::GetOuterDelegateView() {
  RenderViewHostImpl* outer_rvh = static_cast<RenderViewHostImpl*>(
      web_contents_->GetOuterWebContents()->GetRenderViewHost());
  CHECK(outer_rvh);
  return outer_rvh->GetDelegate()->GetDelegateView();
}

gfx::NativeView WebContentsViewChildFrame::GetNativeView() const {
  if (auto* outer_view = GetOuterView()) {
    return outer_view->GetNativeView();
  }

  return gfx::NativeView();
}

gfx::NativeView WebContentsViewChildFrame::GetContentNativeView() const {
  if (auto* outer_view = GetOuterView()) {
    return outer_view->GetContentNativeView();
  }

  return gfx::NativeView();
}

gfx::NativeWindow WebContentsViewChildFrame::GetTopLevelNativeWindow() const {
  if (auto* outer_view = GetOuterView()) {
    return outer_view->GetTopLevelNativeWindow();
  }

  return gfx::NativeWindow();
}

gfx::Rect WebContentsViewChildFrame::GetContainerBounds() const {
  if (RenderWidgetHostView* view = web_contents_->GetRenderWidgetHostView())
    return view->GetViewBounds();

  return gfx::Rect();
}

void WebContentsViewChildFrame::SetInitialFocus() {
  NOTREACHED_IN_MIGRATION();
}

gfx::Rect WebContentsViewChildFrame::GetViewBounds() const {
  NOTREACHED_IN_MIGRATION();
  return gfx::Rect();
}

void WebContentsViewChildFrame::CreateView(gfx::NativeView context) {
  // The WebContentsViewChildFrame does not have a native view.
}

RenderWidgetHostViewBase* WebContentsViewChildFrame::CreateViewForWidget(
    RenderWidgetHost* render_widget_host) {
  return CreateRenderWidgetHostViewForInnerFrameTree(web_contents_,
                                                     render_widget_host);
}

RenderWidgetHostViewBase* WebContentsViewChildFrame::CreateViewForChildWidget(
    RenderWidgetHost* render_widget_host) {
  return GetOuterView()->CreateViewForChildWidget(render_widget_host);
}

void WebContentsViewChildFrame::SetPageTitle(const std::u16string& title) {
  // The title is ignored for the WebContentsViewChildFrame.
}

void WebContentsViewChildFrame::RenderViewReady() {}

void WebContentsViewChildFrame::RenderViewHostChanged(
    RenderViewHost* old_host,
    RenderViewHost* new_host) {}

void WebContentsViewChildFrame::SetOverscrollControllerEnabled(bool enabled) {
  // This is managed by the outer view.
}

#if BUILDFLAG(IS_MAC)
bool WebContentsViewChildFrame::CloseTabAfterEventTrackingIfNeeded() {
  return false;
}
#endif

void WebContentsViewChildFrame::OnCapturerCountChanged() {}

void WebContentsViewChildFrame::FullscreenStateChanged(bool is_fullscreen) {}

void WebContentsViewChildFrame::UpdateWindowControlsOverlay(
    const gfx::Rect& bounding_rect) {}

BackForwardTransitionAnimationManager*
WebContentsViewChildFrame::GetBackForwardTransitionAnimationManager() {
  return nullptr;
}

void WebContentsViewChildFrame::RestoreFocus() {
  NOTREACHED_IN_MIGRATION();
}

void WebContentsViewChildFrame::Focus() {
  NOTREACHED_IN_MIGRATION();
}

void WebContentsViewChildFrame::StoreFocus() {
  NOTREACHED_IN_MIGRATION();
}

void WebContentsViewChildFrame::FocusThroughTabTraversal(bool reverse) {
  NOTREACHED_IN_MIGRATION();
}

DropData* WebContentsViewChildFrame::GetDropData() const {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

void WebContentsViewChildFrame::UpdateDragOperation(
    ui::mojom::DragOperation operation,
    bool document_is_handling_drag) {
  if (auto* view = GetOuterDelegateView()) {
    view->UpdateDragOperation(operation, document_is_handling_drag);
  }
}

void WebContentsViewChildFrame::GotFocus(
    RenderWidgetHostImpl* render_widget_host) {
  NOTREACHED_IN_MIGRATION();
}

void WebContentsViewChildFrame::TakeFocus(bool reverse) {
  // This is handled in RenderFrameHostImpl::TakeFocus we shouldn't
  // end up here.
  NOTREACHED_IN_MIGRATION();
}

void WebContentsViewChildFrame::ShowContextMenu(
    RenderFrameHost& render_frame_host,
    const ContextMenuParams& params) {
  NOTREACHED_IN_MIGRATION();
}

#if BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
void WebContentsViewChildFrame::ShowPopupMenu(
    RenderFrameHost* render_frame_host,
    mojo::PendingRemote<blink::mojom::PopupMenuClient> popup_client,
    const gfx::Rect& bounds,
    int item_height,
    double item_font_size,
    int selected_item,
    std::vector<blink::mojom::MenuItemPtr> menu_items,
    bool right_aligned,
    bool allow_multiple_selection) {
#if BUILDFLAG(IS_MAC)
  NoOpPopupMenuHelperDelegate delegate;
  PopupMenuHelper helper(&delegate, render_frame_host, std::move(popup_client));
  helper.ShowPopupMenu(bounds, item_height, item_font_size, selected_item,
                       std::move(menu_items), right_aligned,
                       allow_multiple_selection);
#endif  // BUILDFLAG(IS_MAC)
}
#endif  // BUILDFLAG(USE_EXTERNAL_POPUP_MENU)

void WebContentsViewChildFrame::StartDragging(
    const DropData& drop_data,
    const url::Origin& source_origin,
    DragOperationsMask ops,
    const gfx::ImageSkia& image,
    const gfx::Vector2d& cursor_offset,
    const gfx::Rect& drag_obj_rect,
    const blink::mojom::DragEventSourceInfo& event_info,
    RenderWidgetHostImpl* source_rwh) {
  if (auto* view = GetOuterDelegateView()) {
    view->StartDragging(drop_data, source_origin, ops, image, cursor_offset,
                        drag_obj_rect, event_info, source_rwh);
  } else {
    web_contents_->GetOuterWebContents()->SystemDragEnded(source_rwh);
  }
}

RenderWidgetHostViewChildFrame*
WebContentsViewChildFrame::CreateRenderWidgetHostViewForInnerFrameTree(
    WebContentsImpl* web_contents,
    RenderWidgetHost* render_widget_host) {
  display::ScreenInfos screen_infos;
  if (auto* view = web_contents->GetRenderWidgetHostView()) {
    screen_infos = view->GetScreenInfos();
  } else {
    display::ScreenInfo screen_info;
    display::DisplayUtil::GetDefaultScreenInfo(&screen_info);
    screen_infos = display::ScreenInfos(screen_info);
  }
  return RenderWidgetHostViewChildFrame::Create(render_widget_host,
                                                screen_infos);
}

}  // namespace content
