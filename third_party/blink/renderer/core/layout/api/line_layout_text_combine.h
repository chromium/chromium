// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_TEXT_COMBINE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_TEXT_COMBINE_H_

#include "third_party/blink/renderer/core/layout/api/line_layout_text.h"
#include "third_party/blink/renderer/core/layout/layout_text_combine.h"

namespace blink {

class LineLayoutTextCombine : public LineLayoutText {
 public:
  explicit LineLayoutTextCombine(LayoutTextCombine* layout_text_combine)
      : LineLayoutText(layout_text_combine) {}

  explicit LineLayoutTextCombine(const LineLayoutItem& item)
      : LineLayoutText(item) {
    SECURITY_DCHECK(!item || item.IsCombineText());
  }

  explicit LineLayoutTextCombine(std::nullptr_t) : LineLayoutText(nullptr) {}

  LineLayoutTextCombine() = default;

  bool IsCombined() const { return ToTextCombine()->IsCombined(); }

 private:
  LayoutTextCombine* ToTextCombine() {
    return To<LayoutTextCombine>(GetLayoutObject());
  }

  const LayoutTextCombine* ToTextCombine() const {
    return To<LayoutTextCombine>(GetLayoutObject());
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_TEXT_COMBINE_H_
