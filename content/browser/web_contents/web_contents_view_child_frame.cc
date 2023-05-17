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
  return web_contents_->GetOuterWebContents()->GetView();
}

const WebContentsView* WebContentsViewChildFrame::GetOuterView() const {
  return web_contents_->GetOuterWebContents()->GetView();
}

RenderViewHostDelegateView* WebContentsViewChildFrame::GetOuterDelegateView() {
  RenderViewHostImpl* outer_rvh = static_cast<RenderViewHostImpl*>(
      web_contents_->GetOuterWebContents()->GetRenderViewHost());
  CHECK(outer_rvh);
  return outer_rvh->GetDelegate()->GetDelegateView();
}

gfx::NativeView WebContentsViewChildFrame::GetNativeView() const {
  return GetOuterView()->GetNativeView();
}

gfx::NativeView WebContentsViewChildFrame::GetContentNativeView() const {
  return GetOuterView()->GetContentNativeView();
}

gfx::NativeWindow WebContentsViewChildFrame::GetTopLevelNativeWindow() const {
  return GetOuterView()->GetTopLevelNativeWindow();
}

gfx::Rect WebContentsViewChildFrame::GetContainerBounds() const {
  if (RenderWidgetHostView* view = web_contents_->GetRenderWidgetHostView())
    return view->GetViewBounds();

  return gfx::Rect();
}

void WebContentsViewChildFrame::SetInitialFocus() {
  NOTREACHED();
}

gfx::Rect WebContentsViewChildFrame::GetViewBounds() const {
  NOTREACHED();
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

void WebContentsViewChildFrame::RestoreFocus() {
  NOTREACHED();
}

void WebContentsViewChildFrame::Focus() {
  NOTREACHED();
}

void WebContentsViewChildFrame::StoreFocus() {
  NOTREACHED();
}

void WebContentsViewChildFrame::FocusThroughTabTraversal(bool reverse) {
  NOTREACHED();
}

DropData* WebContentsViewChildFrame::GetDropData() const {
  NOTREACHED();
  return nullptr;
}

void WebContentsViewChildFrame::UpdateDragCursor(
    ui::mojom::DragOperation operation) {
  if (auto* view = GetOuterDelegateView())
    view->UpdateDragCursor(operation);
}

void WebContentsViewChildFrame::GotFocus(
    RenderWidgetHostImpl* render_widget_host) {
  NOTREACHED();
}

void WebContentsViewChildFrame::TakeFocus(bool reverse) {
  // This is handled in RenderFrameHostImpl::TakeFocus we shouldn't
  // end up here.
  NOTREACHED();
}

void WebContentsViewChildFrame::ShowContextMenu(
    RenderFrameHost& render_frame_host,
    const ContextMenuParams& params) {
  NOTREACHED();
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
    DragOperationsMask ops,
    const gfx::ImageSkia& image,
    const gfx::Vector2d& cursor_offset,
    const gfx::Rect& drag_obj_rect,
    const blink::mojom::DragEventSourceInfo& event_info,
    RenderWidgetHostImpl* source_rwh) {
  if (auto* view = GetOuterDelegateView()) {
    view->StartDragging(drop_data, ops, image, cursor_offset, drag_obj_rect,
                        event_info, source_rwh);
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
