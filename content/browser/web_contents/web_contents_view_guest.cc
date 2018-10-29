// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_view_guest.h"

#include <utility>

#include "base/metrics/user_metrics.h"
#include "build/build_config.h"
#include "content/browser/browser_plugin/browser_plugin_embedder.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/frame_host/interstitial_page_impl.h"
#include "content/browser/frame_host/render_widget_host_view_guest.h"
#include "content/browser/renderer_host/display_util.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/drag_messages.h"
#include "content/public/browser/guest_mode.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/common/drop_data.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

using blink::WebDragOperation;
using blink::WebDragOperationsMask;

namespace content {

WebContentsViewGuest::WebContentsViewGuest(
    WebContentsImpl* web_contents,
    BrowserPluginGuest* guest,
    std::unique_ptr<WebContentsView> platform_view,
    RenderViewHostDelegateView** delegate_view)
    : web_contents_(web_contents),
      guest_(guest),
      platform_view_(std::move(platform_view)),
      platform_view_delegate_view_(*delegate_view) {
  *delegate_view = this;
  DCHECK(!GuestMode::IsCrossProcessFrameGuest(web_contents));
}

WebContentsViewGuest::~WebContentsViewGuest() {
}

gfx::NativeView WebContentsViewGuest::GetNativeView() const {
  return platform_view_->GetNativeView();
}

gfx::NativeView WebContentsViewGuest::GetContentNativeView() const {
  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  if (!rwhv)
    return nullptr;
  return rwhv->GetNativeView();
}

gfx::NativeWindow WebContentsViewGuest::GetTopLevelNativeWindow() const {
  return guest_->embedder_web_contents()->GetTopLevelNativeWindow();
}

void WebContentsViewGuest::OnGuestAttached(WebContentsView* parent_view) {
#if defined(USE_AURA)
  // In aura, ScreenPositionClient doesn't work properly if we do
  // not have the native view associated with this WebContentsViewGuest in the
  // view hierarchy. We add this view as embedder's child here.
  // This would go in WebContentsViewGuest::CreateView, but that is too early to
  // access embedder_web_contents(). Therefore, we do it here.
  if (!features::IsMultiProcessMash())
    parent_view->GetNativeView()->AddChild(platform_view_->GetNativeView());
#endif  // defined(USE_AURA)
}

void WebContentsViewGuest::OnGuestDetached(WebContentsView* old_parent_view) {
#if defined(USE_AURA)
  if (!features::IsMultiProcessMash()) {
    old_parent_view->GetNativeView()->RemoveChild(
        platform_view_->GetNativeView());
  }
#endif  // defined(USE_AURA)
}

void WebContentsViewGuest::GetContainerBounds(gfx::Rect* out) const {
  if (guest_->embedder_web_contents()) {
    // We need embedder container's bounds to calculate our bounds.
    guest_->embedder_web_contents()->GetView()->GetContainerBounds(out);
    gfx::Point guest_coordinates =
        guest_->GetCoordinatesInEmbedderWebContents(gfx::Point());
    out->Offset(guest_coordinates.x(), guest_coordinates.y());
  } else {
    out->set_origin(gfx::Point());
  }

  out->set_size(size_);
}

void WebContentsViewGuest::SizeContents(const gfx::Size& size) {
  size_ = size;
  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  if (rwhv)
    rwhv->SetSize(size);
}

void WebContentsViewGuest::SetInitialFocus() {
  platform_view_->SetInitialFocus();
}

gfx::Rect WebContentsViewGuest::GetViewBounds() const {
  return gfx::Rect(size_);
}

void WebContentsViewGuest::CreateView(const gfx::Size& initial_size,
                                      gfx::NativeView context) {
  platform_view_->CreateView(initial_size, context);
  size_ = initial_size;
}

RenderWidgetHostViewBase* WebContentsViewGuest::CreateViewForWidget(
    RenderWidgetHost* render_widget_host, bool is_guest_view_hack) {
  if (render_widget_host->GetView()) {
    // During testing, the view will already be set up in most cases to the
    // test view, so we don't want to clobber it with a real one. To verify that
    // this actually is happening (and somebody isn't accidentally creating the
    // view twice), we check for the RVH Factory, which will be set when we're
    // making special ones (which go along with the special views).
    DCHECK(RenderViewHostFactory::has_factory());
    return static_cast<RenderWidgetHostViewBase*>(
        render_widget_host->GetView());
  }

  RenderWidgetHostViewBase* platform_widget =
      platform_view_->CreateViewForWidget(render_widget_host, true);

  return RenderWidgetHostViewGuest::Create(render_widget_host, guest_,
                                           platform_widget->GetWeakPtr());
}

RenderWidgetHostViewBase* WebContentsViewGuest::CreateViewForChildWidget(
    RenderWidgetHost* render_widget_host) {
  return platform_view_->CreateViewForChildWidget(render_widget_host);
}

void WebContentsViewGuest::SetPageTitle(const base::string16& title) {
}

void WebContentsViewGuest::RenderViewCreated(RenderViewHost* host) {
  platform_view_->RenderViewCreated(host);
}

void WebContentsViewGuest::RenderViewReady() {
  platform_view_->RenderViewReady();
}

void WebContentsViewGuest::RenderViewHostChanged(RenderViewHost* old_host,
                                                 RenderViewHost* new_host) {
  platform_view_->RenderViewHostChanged(old_host, new_host);
}

void WebContentsViewGuest::SetOverscrollControllerEnabled(bool enabled) {
  // This should never override the setting of the embedder view.
}

#if defined(OS_MACOSX)
bool WebContentsViewGuest::IsEventTracking() const {
  return false;
}

void WebContentsViewGuest::CloseTabAfterEventTracking() {
}
#endif

WebContents* WebContentsViewGuest::web_contents() {
  return web_contents_;
}

void WebContentsViewGuest::RestoreFocus() {
  platform_view_->RestoreFocus();
}

void WebContentsViewGuest::Focus() {
  platform_view_->Focus();
}

void WebContentsViewGuest::StoreFocus() {
  platform_view_->StoreFocus();
}

void WebContentsViewGuest::FocusThroughTabTraversal(bool reverse) {
  platform_view_->FocusThroughTabTraversal(reverse);
}

DropData* WebContentsViewGuest::GetDropData() const {
  NOTIMPLEMENTED();
  return nullptr;
}

void WebContentsViewGuest::UpdateDragCursor(WebDragOperation operation) {
  RenderViewHostImpl* embedder_render_view_host =
      static_cast<RenderViewHostImpl*>(
          guest_->embedder_web_contents()->GetRenderViewHost());
  CHECK(embedder_render_view_host);
  RenderViewHostDelegateView* view =
      embedder_render_view_host->GetDelegate()->GetDelegateView();
  if (view)
    view->UpdateDragCursor(operation);
}

void WebContentsViewGuest::ShowContextMenu(RenderFrameHost* render_frame_host,
                                           const ContextMenuParams& params) {
  DCHECK(platform_view_delegate_view_);
  platform_view_delegate_view_->ShowContextMenu(render_frame_host, params);
}

void WebContentsViewGuest::StartDragging(
    const DropData& drop_data,
    WebDragOperationsMask ops,
    const gfx::ImageSkia& image,
    const gfx::Vector2d& image_offset,
    const DragEventSourceInfo& event_info,
    RenderWidgetHostImpl* source_rwh) {
  WebContentsImpl* embedder_web_contents = guest_->embedder_web_contents();
  embedder_web_contents->GetBrowserPluginEmbedder()->StartDrag(guest_);
  RenderViewHostImpl* embedder_render_view_host =
      static_cast<RenderViewHostImpl*>(
          embedder_web_contents->GetRenderViewHost());
  CHECK(embedder_render_view_host);
  RenderViewHostDelegateView* view =
      embedder_render_view_host->GetDelegate()->GetDelegateView();
  if (view) {
    RecordAction(base::UserMetricsAction("BrowserPlugin.Guest.StartDrag"));
    view->StartDragging(
        drop_data, ops, image, image_offset, event_info, source_rwh);
  } else {
    embedder_web_contents->SystemDragEnded(source_rwh);
  }
}

}  // namespace content
