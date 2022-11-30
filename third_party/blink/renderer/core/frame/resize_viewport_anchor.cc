// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/resize_viewport_anchor.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/root_frame_viewport.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

void ResizeViewportAnchor::ResizeFrameView(const gfx::Size& size) {
  LocalFrameView* frame_view = RootFrameView();
  if (!frame_view)
    return;

  ScrollableArea* root_viewport = frame_view->GetScrollableArea();
  ScrollOffset offset = root_viewport->GetScrollOffset();

  frame_view->Resize(size);
  if (scope_count_ > 0)
    drift_ += root_viewport->GetScrollOffset() - offset;
}

void ResizeViewportAnchor::EndScope() {
  if (--scope_count_ > 0)
    return;

  LocalFrameView* frame_view = RootFrameView();
  if (!frame_view)
    return;

  ScrollOffset visual_viewport_in_document =
      frame_view->GetScrollableArea()->GetScrollOffset() - drift_;

  DCHECK(frame_view->GetRootFrameViewport());
  frame_view->GetRootFrameViewport()->RestoreToAnchor(
      visual_viewport_in_document);

  drift_ = ScrollOffset();
}

LocalFrameView* ResizeViewportAnchor::RootFrameView() {
  if (Frame* frame = page_->MainFrame()) {
    if (LocalFrame* local_frame = DynamicTo<LocalFrame>(frame))
      return local_frame->View();
  }
  return nullptr;
}

}  // namespace blink
