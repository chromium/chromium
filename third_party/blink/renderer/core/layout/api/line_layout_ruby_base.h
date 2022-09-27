// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_RUBY_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_RUBY_BASE_H_

#include "third_party/blink/renderer/core/layout/api/line_layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_ruby_base.h"

namespace blink {

class LineLayoutRubyBase : public LineLayoutBlockFlow {
 public:
  explicit LineLayoutRubyBase(LayoutRubyBase* layout_ruby_base)
      : LineLayoutBlockFlow(layout_ruby_base) {}

  explicit LineLayoutRubyBase(const LineLayoutItem& item)
      : LineLayoutBlockFlow(item) {
    SECURITY_DCHECK(!item || item.IsRubyBase());
  }

  explicit LineLayoutRubyBase(std::nullptr_t) : LineLayoutBlockFlow(nullptr) {}

  LineLayoutRubyBase() = default;

 private:
  LayoutRubyBase* ToRubyBase() { return To<LayoutRubyBase>(GetLayoutObject()); }

  const LayoutRubyBase* ToRubyBase() const {
    return To<LayoutRubyBase>(GetLayoutObject());
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_RUBY_BASE_H_
