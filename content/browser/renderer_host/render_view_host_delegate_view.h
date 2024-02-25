// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_DELEGATE_VIEW_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_DELEGATE_VIEW_H_

#include <vector>

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "content/common/buildflags.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/page/drag_operation.h"
#include "third_party/blink/public/mojom/choosers/popup_menu.mojom.h"
#include "third_party/blink/public/mojom/drag/drag.mojom-forward.h"
#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-forward.h"

namespace blink {
class WebGestureEvent;
}

namespace gfx {
class ImageSkia;
class Rect;
class Vector2d;
}

#if BUILDFLAG(IS_ANDROID)
namespace ui {
class OverscrollRefreshHandler;
}
#endif

namespace url {
class Origin;
}

namespace content {
class RenderFrameHost;
class RenderWidgetHostImpl;
struct ContextMenuParams;
struct DropData;

// This class provides a way for the RenderViewHost to reach out to its
// delegate's view.
class CONTENT_EXPORT RenderViewHostDelegateView {
 public:
  // A context menu should be shown, to be built using the context information
  // provided in the supplied params.
  //
  // The `render_frame_host` represents the frame that requests the context menu
  // (typically this frame is focused, but this is not necessarily the case -
  // see https://crbug.com/1257907#c14).
  virtual void ShowContextMenu(RenderFrameHost& render_frame_host,
                               const ContextMenuParams& params) {}

  // The user started dragging content of the specified type within the
  // `blink::WebView`. Contextual information about the dragged content is
  // supplied by DropData. If the delegate's view cannot start the drag for
  // /any/ reason, it must inform the renderer that the drag has ended;
  // otherwise, this results in bugs like http://crbug.com/157134.
  //
  // The `cursor_offset` parameter is the offset of the cursor (mouse/touch
  // pointer) w.r.t. to top-left corner of the drag-image `image` (in viewport
  // coordinates).  The browser remembers this offset to keep drawing the
  // `image` in a position relative to _current_ drag position (till the end of
  // this drag operation).
  //
  // The `drag_obj_rect` parameter is the extent of the drag-object as rendered
  // on the page (in viewport coordinates).  While this rectangle and the
  // `image` has the same size (width and height), they do not necessaily
  // coincide; below are two example cases when they don't:
  //
  // - For a dragged link, Blink assumes the `image` is horizontally centered
  //   w.r.t. the cursor position.
  //
  // - For a mouse-drag, the top-left corner of `image` is `cursor_offset` away
  //   from the `mousemove` event that started the drag, but the top-left of
  //   `drag_rect_obj` is the same amount away from the `mousedown` event
  //   (except for the dragged link case above when even these two  offsets are
  //   different).  See the function header comment for:
  //   `blink::DragController::StartDrag()`.
  virtual void StartDragging(
      const DropData& drop_data,
      const url::Origin& source_origin,
      blink::DragOperationsMask allowed_ops,
      const gfx::ImageSkia& image,
      const gfx::Vector2d& cursor_offset,
      const gfx::Rect& drag_obj_rect,
      const blink::mojom::DragEventSourceInfo& event_info,
      RenderWidgetHostImpl* source_rwh) {}

  // The page wants to update the mouse cursor during a drag & drop operation.
  // `operation` describes the current operation (none, move, copy, link.).
  // `document_is_handling_drag` describes if the document is handling the
  // drop.
  virtual void UpdateDragOperation(ui::mojom::DragOperation operation,
                                   bool document_is_handling_drag) {}

  // Notification that view for this delegate got the focus.
  virtual void GotFocus(RenderWidgetHostImpl* render_widget_host) {}

  // Notification that view for this delegate lost the focus.
  virtual void LostFocus(RenderWidgetHostImpl* render_widget_host) {}

  // Callback to inform the browser that the page is returning the focus to
  // the browser's chrome. If reverse is true, it means the focus was
  // retrieved by doing a Shift-Tab.
  virtual void TakeFocus(bool reverse) {}

  // Returns the height of the top controls in physical pixels (not DIPs).
  virtual int GetTopControlsHeight() const;

  // Returns the minimum visible height the top controls can have in physical
  // pixels (not DIPs).
  virtual int GetTopControlsMinHeight() const;

  // Returns the height of the bottom controls in physical pixels (not DIPs).
  virtual int GetBottomControlsHeight() const;

  // Returns the minimum visible height the bottom controls can have in physical
  // pixels (not DIPs).
  virtual int GetBottomControlsMinHeight() const;

  // Returns true if the changes in browser controls height (including min
  // height) should be animated.
  virtual bool ShouldAnimateBrowserControlsHeightChanges() const;

  // Returns true if the browser controls resize the renderer's view size.
  virtual bool DoBrowserControlsShrinkRendererSize() const;

  // Returns true if the top controls should only expand at the top of the page,
  // so they'll only be visible if the page is scrolled to the top.
  virtual bool OnlyExpandTopControlsAtPageTop() const;

  // Do post-event tasks for gesture events.
  virtual void GestureEventAck(const blink::WebGestureEvent& event,
                               blink::mojom::InputEventResultState ack_result);

#if BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
  // Shows a popup menu with the specified items.
  // This method should call
  // blink::mojom::PopupMenuClient::DidAcceptIndices() or
  // blink::mojom::PopupMenuClient::DidCancel() based on the user action.
  virtual void ShowPopupMenu(
      RenderFrameHost* render_frame_host,
      mojo::PendingRemote<blink::mojom::PopupMenuClient> popup_client,
      const gfx::Rect& bounds,
      int item_height,
      double item_font_size,
      int selected_item,
      std::vector<blink::mojom::MenuItemPtr> menu_items,
      bool right_aligned,
      bool allow_multiple_selection) {}
#endif

#if BUILDFLAG(IS_ANDROID)
  virtual ui::OverscrollRefreshHandler* GetOverscrollRefreshHandler() const;
#endif

 protected:
  virtual ~RenderViewHostDelegateView() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_DELEGATE_VIEW_H_
