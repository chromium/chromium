// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_GUEST_H_
#define CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_GUEST_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/common/content_export.h"
#include "content/common/drag_event_source_info.h"

namespace content {

class WebContents;
class WebContentsImpl;
class BrowserPluginGuest;

class WebContentsViewGuest : public WebContentsView,
                             public RenderViewHostDelegateView {
 public:
  // The corresponding WebContentsImpl is passed in the constructor, and manages
  // our lifetime. This doesn't need to be the case, but is this way currently
  // because that's what was easiest when they were split.
  // WebContentsViewGuest always has a backing platform dependent view,
  // |platform_view|.
  WebContentsViewGuest(WebContentsImpl* web_contents,
                       BrowserPluginGuest* guest,
                       std::unique_ptr<WebContentsView> platform_view,
                       RenderViewHostDelegateView** delegate_view);
  ~WebContentsViewGuest() override;

  WebContents* web_contents();

  void OnGuestAttached(WebContentsView* parent_view);

  void OnGuestDetached(WebContentsView* old_parent_view);

  // WebContentsView implementation --------------------------------------------
  gfx::NativeView GetNativeView() const override;
  gfx::NativeView GetContentNativeView() const override;
  gfx::NativeWindow GetTopLevelNativeWindow() const override;
  void GetContainerBounds(gfx::Rect* out) const override;
  void SizeContents(const gfx::Size& size) override;
  void Focus() override;
  void SetInitialFocus() override;
  void StoreFocus() override;
  void RestoreFocus() override;
  void FocusThroughTabTraversal(bool reverse) override;
  DropData* GetDropData() const override;
  gfx::Rect GetViewBounds() const override;
  void CreateView(const gfx::Size& initial_size,
                  gfx::NativeView context) override;
  RenderWidgetHostViewBase* CreateViewForWidget(
      RenderWidgetHost* render_widget_host,
      bool is_guest_view_hack) override;
  RenderWidgetHostViewBase* CreateViewForChildWidget(
      RenderWidgetHost* render_widget_host) override;
  void SetPageTitle(const base::string16& title) override;
  void RenderViewCreated(RenderViewHost* host) override;
  void RenderViewReady() override;
  void RenderViewHostChanged(RenderViewHost* old_host,
                             RenderViewHost* new_host) override;
  void SetOverscrollControllerEnabled(bool enabled) override;
#if defined(OS_MACOSX)
  bool IsEventTracking() const override;
  void CloseTabAfterEventTracking() override;
#endif

  // Backend implementation of RenderViewHostDelegateView.
  void ShowContextMenu(RenderFrameHost* render_frame_host,
                       const ContextMenuParams& params) override;
  void StartDragging(const DropData& drop_data,
                     blink::WebDragOperationsMask allowed_ops,
                     const gfx::ImageSkia& image,
                     const gfx::Vector2d& image_offset,
                     const DragEventSourceInfo& event_info,
                     RenderWidgetHostImpl* source_rwh) override;
  void UpdateDragCursor(blink::WebDragOperation operation) override;

 private:
  // The WebContentsImpl whose contents we display.
  WebContentsImpl* web_contents_;
  BrowserPluginGuest* guest_;
  // The platform dependent view backing this WebContentsView.
  // Calls to this WebContentsViewGuest are forwarded to |platform_view_|.
  std::unique_ptr<WebContentsView> platform_view_;
  gfx::Size size_;

  // Delegate view for guest's platform view.
  RenderViewHostDelegateView* platform_view_delegate_view_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsViewGuest);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_GUEST_H_
