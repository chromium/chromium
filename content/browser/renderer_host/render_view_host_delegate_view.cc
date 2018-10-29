// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_view_host_delegate_view.h"

namespace content {

#if defined(OS_ANDROID)
ui::OverscrollRefreshHandler*
content::RenderViewHostDelegateView::GetOverscrollRefreshHandler() const {
  return nullptr;
}
#endif

int RenderViewHostDelegateView::GetTopControlsHeight() const {
  return 0;
}

int RenderViewHostDelegateView::GetBottomControlsHeight() const {
  return 0;
}

bool RenderViewHostDelegateView::DoBrowserControlsShrinkRendererSize() const {
  return false;
}

void RenderViewHostDelegateView::GestureEventAck(
    const blink::WebGestureEvent& event,
    InputEventAckState ack_result) {}

}  //  namespace content
