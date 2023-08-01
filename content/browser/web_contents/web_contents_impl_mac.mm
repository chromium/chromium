// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_impl.h"

#import <Cocoa/Cocoa.h>

#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/size.h"

namespace content {

void WebContentsImpl::Resize(const gfx::Rect& new_bounds) {
  NSView* view = GetNativeView().GetNativeNSView();
  NSRect old_wcv_frame = view.frame;
  CGFloat new_x = old_wcv_frame.origin.x;
  CGFloat new_y = old_wcv_frame.origin.y +
                  (old_wcv_frame.size.height - new_bounds.size().height());
  NSRect new_wcv_frame = NSMakeRect(new_x, new_y, new_bounds.size().width(),
                                    new_bounds.size().height());
  view.frame = new_wcv_frame;
}

gfx::Size WebContentsImpl::GetSize() {
  NSView* view = GetNativeView().GetNativeNSView();
  NSRect frame = view.frame;
  return gfx::Size(NSWidth(frame), NSHeight(frame));
}

}  // namespace content
