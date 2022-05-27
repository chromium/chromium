// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_INLINE_PAINT_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_INLINE_PAINT_CONTEXT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/paint/ng/ng_decorating_box.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// Carries contextual information shared across multiple inline fragments within
// an inline formatting context.
class CORE_EXPORT NGInlinePaintContext {
 public:
  using DecoratingBoxList = Vector<NGDecoratingBox, 1>;
  const DecoratingBoxList& DecoratingBoxes() const { return decorating_boxes_; }

  void PushDecoratingBox(const NGFragmentItem& inline_box,
                         const ComputedStyle& style);
  void PopDecoratingBox() { decorating_boxes_.pop_back(); }

  void SetLineBox(const NGFragmentItem& line_item);
  void ClearLineBox() { line_item_ = nullptr; }

 private:
  DecoratingBoxList decorating_boxes_;
  const NGFragmentItem* line_item_ = nullptr;
};

inline void NGInlinePaintContext::PushDecoratingBox(
    const NGFragmentItem& inline_box,
    const ComputedStyle& style) {
  DCHECK(inline_box.IsInlineBox());
  DCHECK_EQ(&inline_box.Style(), &style);
  decorating_boxes_.emplace_back(inline_box, style);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_INLINE_PAINT_CONTEXT_H_
