// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_RUBY_TEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_RUBY_TEXT_H_

#include "third_party/blink/renderer/core/layout/api/line_layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_ruby_text.h"

namespace blink {

class LineLayoutRubyText : public LineLayoutBlockFlow {
 public:
  explicit LineLayoutRubyText(LayoutRubyText* layout_ruby_text)
      : LineLayoutBlockFlow(layout_ruby_text) {}

  explicit LineLayoutRubyText(const LineLayoutItem& item)
      : LineLayoutBlockFlow(item) {
    SECURITY_DCHECK(!item || item.IsRubyText());
  }

  explicit LineLayoutRubyText(std::nullptr_t) : LineLayoutBlockFlow(nullptr) {}

  LineLayoutRubyText() = default;

 private:
  LayoutRubyText* ToRubyText() { return To<LayoutRubyText>(GetLayoutObject()); }

  const LayoutRubyText* ToRubyText() const {
    return To<LayoutRubyText>(GetLayoutObject());
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_RUBY_TEXT_H_
