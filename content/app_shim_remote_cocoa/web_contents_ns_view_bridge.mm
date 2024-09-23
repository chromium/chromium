// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app_shim_remote_cocoa/web_contents_ns_view_bridge.h"

#include "base/apple/foundation_util.h"
#import "base/task/sequenced_task_runner.h"
#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"
#include "components/remote_cocoa/app_shim/ns_view_ids.h"
#import "content/app_shim_remote_cocoa/web_contents_view_cocoa.h"
#include "content/browser/web_contents/web_contents_view_mac.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#include "ui/gfx/image/image_skia_util_mac.h"

namespace remote_cocoa {

WebContentsNSViewBridge::WebContentsNSViewBridge(
    uint64_t view_id,
    mojo::PendingAssociatedRemote<mojom::WebContentsNSViewHost> client)
    : host_(std::move(client),
            ui::WindowResizeHelperMac::Get()->task_runner()) {
  ns_view_ = [[WebContentsViewCocoa alloc] initWithViewsHostableView:nullptr];
  [ns_view_ setHost:host_.get()];
  [ns_view_ enableDroppedScreenShotCopier];
  view_id_ =
      std::make_unique<remote_cocoa::ScopedNSViewIdMapping>(view_id, ns_view_);
}

WebContentsNSViewBridge::WebContentsNSViewBridge(
    uint64_t view_id,
    content::WebContentsViewMac* web_contents_view) {
  ns_view_ = [[WebContentsViewCocoa alloc]
      initWithViewsHostableView:web_contents_view];
  [ns_view_ setHost:web_contents_view];
  view_id_ =
      std::make_unique<remote_cocoa::ScopedNSViewIdMapping>(view_id, ns_view_);
}

WebContentsNSViewBridge::~WebContentsNSViewBridge() {
  // This handles the case where a renderer close call was deferred
  // while the user was operating a UI control which resulted in a
  // close.  In that case, the Cocoa view outlives the
  // WebContentsViewMac instance due to Cocoa retain count.
  [ns_view_ setHost:nullptr];
  [ns_view_ clearViewsHostableView];
  [ns_view_ removeFromSuperview];
}

void WebContentsNSViewBridge::Bind(
    mojo::PendingAssociatedReceiver<mojom::WebContentsNSView> receiver,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  receiver_.Bind(std::move(receiver), std::move(task_runner));
  receiver_.set_disconnect_handler(base::BindOnce(
      &WebContentsNSViewBridge::Destroy, base::Unretained(this)));
}

void WebContentsNSViewBridge::Destroy() {
  delete this;
}

void WebContentsNSViewBridge::SetParentNSView(uint64_t parent_ns_view_id) {
  NSView* parent_ns_view = remote_cocoa::GetNSViewFromId(parent_ns_view_id);
  // If the browser passed an invalid handle, then there is no recovery.
  CHECK(parent_ns_view);
  [parent_ns_view addSubview:ns_view_];
}

void WebContentsNSViewBridge::ResetParentNSView() {
  [ns_view_ removeFromSuperview];
}

void WebContentsNSViewBridge::SetBounds(const gfx::Rect& bounds_in_window) {
  NSWindow* window = [ns_view_ window];
  NSRect window_content_rect = [window contentRectForFrameRect:[window frame]];
  NSRect ns_bounds_in_window =
      NSMakeRect(bounds_in_window.x(),
                 window_content_rect.size.height - bounds_in_window.y() -
                     bounds_in_window.height(),
                 bounds_in_window.width(), bounds_in_window.height());
  NSRect ns_bounds_in_superview =
      [[ns_view_ superview] convertRect:ns_bounds_in_window fromView:nil];
  [ns_view_ setFrame:ns_bounds_in_superview];
}

void WebContentsNSViewBridge::SetVisible(bool visible) {
  // If the first responder is a child of the current view, AppKit will search
  // for a new first responder during `-setHidden:`. The key view loop is
  // searched for a view that can become key. Typically this search yields no
  // results and the window becomes the default first responder. However if this
  // occurs after an immersive fullscreen restore an infinite loop can occur
  // leading to an OOM. This occurs because of the existence of an NSToolbar,
  // which causes the key loop traversal to jump back and forth between the
  // view's window and the AppKit owned NSToolbarFullscreenWindow which hosts
  // the toolbar in immersive fullscreen. To prevent this set the window's first
  // responder to nil which will make the window the first responder before the
  // hide.
  // TODO(http://crbug.com/40261565): Remove when FB12010731 is fixed in
  // AppKit.
  NativeWidgetMacNSWindow* widget_window =
      base::apple::ObjCCast<NativeWidgetMacNSWindow>(ns_view_.window);
  if (!visible && [widget_window immersiveFullscreen]) {
    NSView* first_responder =
        base::apple::ObjCCast<NSView>(ns_view_.window.firstResponder);
    if ([first_responder isDescendantOf:ns_view_]) {
      [ns_view_.window makeFirstResponder:nil];
    }
  }

  [ns_view_ setHidden:!visible];
}

void WebContentsNSViewBridge::MakeFirstResponder() {
  if ([ns_view_ acceptsFirstResponder])
    [[ns_view_ window] makeFirstResponder:ns_view_];
}

void WebContentsNSViewBridge::TakeFocus(bool reverse) {
  if (reverse)
    [[ns_view_ window] selectPreviousKeyView:ns_view_];
  else
    [[ns_view_ window] selectNextKeyView:ns_view_];
}

void WebContentsNSViewBridge::StartDrag(const content::DropData& drop_data,
                                        const url::Origin& source_origin,
                                        uint32_t operation_mask,
                                        const gfx::ImageSkia& image,
                                        const gfx::Vector2d& image_offset,
                                        bool is_privileged) {
  NSPoint offset = NSPointFromCGPoint(
      gfx::PointAtOffsetFromOrigin(image_offset).ToCGPoint());
  [ns_view_ startDragWithDropData:drop_data
                     sourceOrigin:source_origin
                dragOperationMask:operation_mask
                            image:gfx::NSImageFromImageSkia(image)
                           offset:offset
                     isPrivileged:is_privileged];
}

void WebContentsNSViewBridge::UpdateWindowControlsOverlay(
    const gfx::Rect& bounding_rect) {
  [ns_view_ updateWindowControlsOverlay:bounding_rect];
}

}  // namespace remote_cocoa
