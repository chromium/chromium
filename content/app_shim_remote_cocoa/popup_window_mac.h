// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_APP_SHIM_REMOTE_COCOA_POPUP_WINDOW_MAC_H_
#define CONTENT_APP_SHIM_REMOTE_COCOA_POPUP_WINDOW_MAC_H_

#import "base/mac/scoped_nsobject.h"
#include "content/public/common/widget_type.h"
#include "ui/gfx/geometry/rect.h"

@class NSWindow;
@class RenderWidgetHostViewCocoa;

namespace remote_cocoa {

// Helper class for RHWVMacs that are initialized using InitAsPopup. Note that
// this refers to UI that creates its own NSWindow, and does not refer to JS
// initiated popups. This can be tesed using <input type="datetime-local">.
class PopupWindowMac {
 public:
  PopupWindowMac(const gfx::Rect& content_rect,
                 RenderWidgetHostViewCocoa* cocoa_view);

  PopupWindowMac(const PopupWindowMac&) = delete;
  PopupWindowMac& operator=(const PopupWindowMac&) = delete;

  ~PopupWindowMac();

  NSWindow* window() { return popup_window_.get(); }

 private:
  base::scoped_nsobject<NSWindow> popup_window_;

  // Weak.
  RenderWidgetHostViewCocoa* cocoa_view_ = nil;
};

}  // namespace remote_cocoa

#endif  // CONTENT_APP_SHIM_REMOTE_COCOA_POPUP_WINDOW_MAC_H_
