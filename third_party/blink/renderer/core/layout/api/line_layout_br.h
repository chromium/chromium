// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_BR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_BR_H_

#include "third_party/blink/renderer/core/layout/api/line_layout_text.h"
#include "third_party/blink/renderer/core/layout/layout_br.h"

namespace blink {

class LineLayoutBR : public LineLayoutText {
 public:
  explicit LineLayoutBR(LayoutBR* layout_br) : LineLayoutText(layout_br) {}

  explicit LineLayoutBR(const LineLayoutItem& item) : LineLayoutText(item) {
    SECURITY_DCHECK(!item || item.IsBR());
  }

  explicit LineLayoutBR(std::nullptr_t) : LineLayoutText(nullptr) {}

  LineLayoutBR() = default;

  int LineHeight(bool first_line) const {
    return ToBR()->LineHeight(first_line);
  }

 private:
  LayoutBR* ToBR() { return To<LayoutBR>(GetLayoutObject()); }

  const LayoutBR* ToBR() const { return To<LayoutBR>(GetLayoutObject()); }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_BR_H_
