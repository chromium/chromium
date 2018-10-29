// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_view_child_frame.h"

#include "build/build_config.h"
#include "content/browser/frame_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/display_util.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

using blink::WebDragOperation;
using blink::WebDragOperationsMask;

namespace content {

WebContentsViewChildFrame::WebContentsViewChildFrame(
    WebContentsImpl* web_contents,
    WebContentsViewDelegate* delegate,
    RenderViewHostDelegateView** delegate_view)
    : web_contents_(web_contents),
    delegate_(delegate) {
  *delegate_view = this;
}

WebContentsViewChildFrame::~WebContentsViewChildFrame() {}

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

void WebContentsViewChildFrame::GetContainerBounds(gfx::Rect* out) const {
  RenderWidgetHostView* view = web_contents_->GetRenderWidgetHostView();
  if (view)
    *out = view->GetViewBounds();
  else
    *out = gfx::Rect();
}

void WebContentsViewChildFrame::SizeContents(const gfx::Size& size) {
  // The RenderWidgetHostViewChildFrame is responsible for sizing the contents.
}

void WebContentsViewChildFrame::SetInitialFocus() {
  NOTREACHED();
}

gfx::Rect WebContentsViewChildFrame::GetViewBounds() const {
  NOTREACHED();
  return gfx::Rect();
}

void WebContentsViewChildFrame::CreateView(const gfx::Size& initial_size,
                                           gfx::NativeView context) {
  // The WebContentsViewChildFrame does not have a native view.
}

RenderWidgetHostViewBase* WebContentsViewChildFrame::CreateViewForWidget(
    RenderWidgetHost* render_widget_host,
    bool is_guest_view_hack) {
  return RenderWidgetHostViewChildFrame::Create(render_widget_host);
}

RenderWidgetHostViewBase* WebContentsViewChildFrame::CreateViewForChildWidget(
    RenderWidgetHost* render_widget_host) {
  return GetOuterView()->CreateViewForChildWidget(render_widget_host);
}

void WebContentsViewChildFrame::SetPageTitle(const base::string16& title) {
  // The title is ignored for the WebContentsViewChildFrame.
}

void WebContentsViewChildFrame::RenderViewCreated(RenderViewHost* host) {}

void WebContentsViewChildFrame::RenderViewReady() {}

void WebContentsViewChildFrame::RenderViewHostChanged(
    RenderViewHost* old_host,
    RenderViewHost* new_host) {}

void WebContentsViewChildFrame::SetOverscrollControllerEnabled(bool enabled) {
  // This is managed by the outer view.
}

#if defined(OS_MACOSX)
bool WebContentsViewChildFrame::IsEventTracking() const {
  return false;
}

void WebContentsViewChildFrame::CloseTabAfterEventTracking() {
  NOTREACHED();
}
#endif

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

void WebContentsViewChildFrame::UpdateDragCursor(WebDragOperation operation) {
  if (auto* view = GetOuterDelegateView())
    view->UpdateDragCursor(operation);
}

void WebContentsViewChildFrame::GotFocus(
    RenderWidgetHostImpl* render_widget_host) {
  NOTREACHED();
}

void WebContentsViewChildFrame::TakeFocus(bool reverse) {
  RenderFrameProxyHost* rfp = web_contents_->GetMainFrame()
                                  ->frame_tree_node()
                                  ->render_manager()
                                  ->GetProxyToOuterDelegate();
  FrameTreeNode* outer_node = FrameTreeNode::GloballyFindByID(
      web_contents_->GetOuterDelegateFrameTreeNodeId());
  RenderFrameHostImpl* rfhi =
      outer_node->parent()->render_manager()->current_frame_host();

  rfhi->AdvanceFocus(
      reverse ? blink::kWebFocusTypeBackward : blink::kWebFocusTypeForward,
      rfp);
}

void WebContentsViewChildFrame::ShowContextMenu(
    RenderFrameHost* render_frame_host,
    const ContextMenuParams& params) {
  NOTREACHED();
}

void WebContentsViewChildFrame::StartDragging(
    const DropData& drop_data,
    WebDragOperationsMask ops,
    const gfx::ImageSkia& image,
    const gfx::Vector2d& image_offset,
    const DragEventSourceInfo& event_info,
    RenderWidgetHostImpl* source_rwh) {
  if (auto* view = GetOuterDelegateView()) {
    view->StartDragging(
        drop_data, ops, image, image_offset, event_info, source_rwh);
  } else {
    web_contents_->GetOuterWebContents()->SystemDragEnded(source_rwh);
  }
}

}  // namespace content
