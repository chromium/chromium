// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_DELEGATE_VIEW_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_DELEGATE_VIEW_H_

#include <vector>

#include "base/callback.h"
#include "build/build_config.h"
#include "content/common/buildflags.h"
#include "content/common/content_export.h"
#include "content/common/drag_event_source_info.h"
#include "content/public/common/input_event_ack_state.h"
#include "third_party/blink/public/platform/web_drag_operation.h"

namespace blink {
class WebGestureEvent;
}

namespace gfx {
class ImageSkia;
class Rect;
class Vector2d;
}

#if defined(OS_ANDROID)
namespace ui {
class OverscrollRefreshHandler;
}
#endif

namespace content {
class RenderFrameHost;
class RenderWidgetHostImpl;
struct ContextMenuParams;
struct DropData;
struct MenuItem;

// This class provides a way for the RenderViewHost to reach out to its
// delegate's view.
class CONTENT_EXPORT RenderViewHostDelegateView {
 public:
  // A context menu should be shown, to be built using the context information
  // provided in the supplied params.
  virtual void ShowContextMenu(RenderFrameHost* render_frame_host,
                               const ContextMenuParams& params) {}

  // The user started dragging content of the specified type within the
  // RenderView. Contextual information about the dragged content is supplied
  // by DropData. If the delegate's view cannot start the drag for /any/
  // reason, it must inform the renderer that the drag has ended; otherwise,
  // this results in bugs like http://crbug.com/157134.
  virtual void StartDragging(const DropData& drop_data,
                             blink::WebDragOperationsMask allowed_ops,
                             const gfx::ImageSkia& image,
                             const gfx::Vector2d& image_offset,
                             const DragEventSourceInfo& event_info,
                             RenderWidgetHostImpl* source_rwh) {}

  // The page wants to update the mouse cursor during a drag & drop operation.
  // |operation| describes the current operation (none, move, copy, link.)
  virtual void UpdateDragCursor(blink::WebDragOperation operation) {}

  // Notification that view for this delegate got the focus.
  virtual void GotFocus(RenderWidgetHostImpl* render_widget_host) {}

  // Notification that view for this delegate lost the focus.
  virtual void LostFocus(RenderWidgetHostImpl* render_widget_host) {}

  // Callback to inform the browser that the page is returning the focus to
  // the browser's chrome. If reverse is true, it means the focus was
  // retrieved by doing a Shift-Tab.
  virtual void TakeFocus(bool reverse) {}

  // Returns the height of the top controls in DIP.
  virtual int GetTopControlsHeight() const;

  // Returns the height of the bottom controls in DIP.
  virtual int GetBottomControlsHeight() const;

  // Returns true if the browser controls resize the renderer's view size.
  virtual bool DoBrowserControlsShrinkRendererSize() const;

  // Do post-event tasks for gesture events.
  virtual void GestureEventAck(const blink::WebGestureEvent& event,
                               InputEventAckState ack_result);

#if BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
  // Shows a popup menu with the specified items.
  // This method should call RenderFrameHost::DidSelectPopupMenuItem[s]() or
  // RenderFrameHost::DidCancelPopupMenu() based on the user action.
  virtual void ShowPopupMenu(RenderFrameHost* render_frame_host,
                             const gfx::Rect& bounds,
                             int item_height,
                             double item_font_size,
                             int selected_item,
                             const std::vector<MenuItem>& items,
                             bool right_aligned,
                             bool allow_multiple_selection) {};

  // Hides a popup menu opened by ShowPopupMenu().
  virtual void HidePopupMenu() {};
#endif

#if defined(OS_ANDROID)
  virtual ui::OverscrollRefreshHandler* GetOverscrollRefreshHandler() const;
#endif

 protected:
  virtual ~RenderViewHostDelegateView() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_DELEGATE_VIEW_H_
