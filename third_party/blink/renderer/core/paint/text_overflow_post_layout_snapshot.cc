// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/text_overflow_post_layout_snapshot.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_invalidation_reason.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"

namespace blink {

TextOverflowPostLayoutSnapshot::TextOverflowPostLayoutSnapshot(
    PaintLayerScrollableArea& scroller)
    : PostLayoutSnapshotClient(
          scroller.GetLayoutBox()->GetDocument().GetFrame()),
      scroller_(&scroller) {
  scroller_->RegisterTextOverflowPostLayoutSnapshot(this);
}

bool TextOverflowPostLayoutSnapshot::UpdateSnapshot() {
  if (LayoutBox* box = scroller_->GetLayoutBox()) {
    const bool is_scrolled = IsScrolled();
    if (was_scrolled_ != is_scrolled) {
      was_scrolled_ = is_scrolled;
      box->SetNeedsLayout(layout_invalidation_reason::kUnknown);
      return true;
    }
  }
  return false;
}

bool TextOverflowPostLayoutSnapshot::IsScrolled() const {
  if (const LayoutBox* box = scroller_->GetLayoutBox()) {
    ScrollOffset offset = scroller_->GetScrollOffset();
    return box->IsHorizontalWritingMode() ? offset.x() : offset.y();
  }
  return false;
}

bool TextOverflowPostLayoutSnapshot::ShouldScheduleNextService() {
  return false;
}

void TextOverflowPostLayoutSnapshot::Trace(Visitor* visitor) const {
  visitor->Trace(scroller_);
  PostLayoutSnapshotClient::Trace(visitor);
}

}  // namespace blink
