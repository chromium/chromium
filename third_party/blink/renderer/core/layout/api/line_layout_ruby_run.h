// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_RUBY_RUN_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_RUBY_RUN_H_

#include "third_party/blink/renderer/core/layout/api/line_layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_ruby_base.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_ruby_text.h"
#include "third_party/blink/renderer/core/layout/layout_ruby_run.h"

namespace blink {

class LineLayoutRubyRun : public LineLayoutBlockFlow {
 public:
  explicit LineLayoutRubyRun(LayoutRubyRun* layout_ruby_run)
      : LineLayoutBlockFlow(layout_ruby_run) {}

  explicit LineLayoutRubyRun(const LineLayoutItem& item)
      : LineLayoutBlockFlow(item) {
    SECURITY_DCHECK(!item || item.IsRubyRun());
  }

  explicit LineLayoutRubyRun(std::nullptr_t) : LineLayoutBlockFlow(nullptr) {}

  LineLayoutRubyRun() = default;

  void GetOverhang(bool first_line,
                   LineLayoutItem start_layout_item,
                   LineLayoutItem end_layout_item,
                   int& start_overhang,
                   int& end_overhang) const {
    ToRubyRun()->GetOverhang(first_line, start_layout_item.GetLayoutObject(),
                             end_layout_item.GetLayoutObject(), start_overhang,
                             end_overhang);
  }

  LineLayoutRubyText RubyText() const {
    return LineLayoutRubyText(ToRubyRun()->RubyText());
  }

  LineLayoutRubyBase RubyBase() const {
    return LineLayoutRubyBase(ToRubyRun()->RubyBase());
  }

  bool CanBreakBefore(const LazyLineBreakIterator& iterator) const {
    return ToRubyRun()->CanBreakBefore(iterator);
  }

 private:
  LayoutRubyRun* ToRubyRun() { return To<LayoutRubyRun>(GetLayoutObject()); }

  const LayoutRubyRun* ToRubyRun() const {
    return To<LayoutRubyRun>(GetLayoutObject());
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_RUBY_RUN_H_
