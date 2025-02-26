// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/plain_text_node.h"

#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/text/text_run.h"

namespace blink {

void PlainTextItem::Trace(Visitor* visitor) const {
  visitor->Trace(shape_result_);
  visitor->Trace(shape_result_view_);
}

const ShapeResultView* PlainTextItem::EnsureView() const {
  if (shape_result_ && !shape_result_view_) {
    shape_result_view_ = ShapeResultView::Create(shape_result_);
  }
  return shape_result_view_;
}

// ================================================================

PlainTextNode::PlainTextNode(const TextRun& run,
                             bool normalize_space,
                             bool bidi_overridden,
                             const Font& font,
                             bool supports_bidi)
    : base_direction_(run.Direction()) {
  // TODO(crbug.com/389726691): Implement segmentation and shaping.
}

void PlainTextNode::Trace(Visitor* visitor) const {
  visitor->Trace(item_list_);
}

}  // namespace blink
