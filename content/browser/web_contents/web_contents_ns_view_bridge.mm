// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_ns_view_bridge.h"

#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"

namespace content {

WebContentsNSViewBridge::WebContentsNSViewBridge(
    uint64_t view_id,
    mojom::WebContentsNSViewClientAssociatedPtr client,
    mojom::WebContentsNSViewBridgeAssociatedRequest bridge_request)
    : client_(std::move(client)), binding_(this) {
  binding_.Bind(std::move(bridge_request),
                ui::WindowResizeHelperMac::Get()->task_runner());
  // This object will be destroyed when its connection is closed.
  binding_.set_connection_error_handler(base::BindOnce(
      &WebContentsNSViewBridge::OnConnectionError, base::Unretained(this)));

  // Note that this is an ordinary NSView (as opposed to a full
  // WebContentsViewCocoa).
  cocoa_view_.reset([[NSView alloc] initWithFrame:NSZeroRect]);
  view_id_ =
      std::make_unique<ui::ScopedNSViewIdMapping>(view_id, cocoa_view_.get());
}

WebContentsNSViewBridge::~WebContentsNSViewBridge() {
  [cocoa_view_ removeFromSuperview];
}

void WebContentsNSViewBridge::OnConnectionError() {
  delete this;
}

void WebContentsNSViewBridge::SetParentViewsNSView(uint64_t parent_ns_view_id) {
  NSView* parent_ns_view = ui::NSViewIds::GetNSView(parent_ns_view_id);
  // If the browser passed an invalid handle, then there is no recovery.
  CHECK(parent_ns_view);
  [parent_ns_view addSubview:cocoa_view_];
}

void WebContentsNSViewBridge::Show(const gfx::Rect& bounds_in_window) {
  NSRect ns_bounds_in_window =
      NSMakeRect(bounds_in_window.x(),
                 [[[cocoa_view_ window] contentView] frame].size.height -
                     bounds_in_window.y() - bounds_in_window.height(),
                 bounds_in_window.width(), bounds_in_window.height());
  NSRect ns_bounds_in_superview =
      [[cocoa_view_ superview] convertRect:ns_bounds_in_window fromView:nil];
  [cocoa_view_ setFrame:ns_bounds_in_superview];
  // Ensure that the child RenderWidgetHostViews have the same frame as the
  // WebContentsView.
  for (NSView* child in [cocoa_view_ subviews])
    [child setFrame:[cocoa_view_ bounds]];
  [cocoa_view_ setHidden:NO];
}

void WebContentsNSViewBridge::Hide() {
  [cocoa_view_ setHidden:YES];
}

void WebContentsNSViewBridge::MakeFirstResponder() {
  if ([cocoa_view_ acceptsFirstResponder])
    [[cocoa_view_ window] makeFirstResponder:cocoa_view_];
}

}  // namespace content
