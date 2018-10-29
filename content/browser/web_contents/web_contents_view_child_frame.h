// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_CHILD_FRAME_H_
#define CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_CHILD_FRAME_H_

#include "base/macros.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/web_contents/web_contents_view.h"

namespace content {

class RenderWidgetHostImpl;
class WebContentsImpl;
class WebContentsViewDelegate;

class WebContentsViewChildFrame : public WebContentsView,
                                  public RenderViewHostDelegateView {
 public:
  WebContentsViewChildFrame(WebContentsImpl* web_contents,
                            WebContentsViewDelegate* delegate,
                            RenderViewHostDelegateView** delegate_view);
  ~WebContentsViewChildFrame() override;

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
  void GotFocus(RenderWidgetHostImpl* render_widget_host) override;
  void TakeFocus(bool reverse) override;

 private:
  WebContentsView* GetOuterView();
  const WebContentsView* GetOuterView() const;

  RenderViewHostDelegateView* GetOuterDelegateView();

  // The WebContentsImpl whose contents we display.
  WebContentsImpl* web_contents_;

  // The delegate ownership is passed to WebContentsView.
  std::unique_ptr<WebContentsViewDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsViewChildFrame);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_CHILD_FRAME_H_
