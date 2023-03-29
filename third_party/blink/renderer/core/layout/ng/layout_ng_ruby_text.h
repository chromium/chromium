// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_RUBY_TEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_RUBY_TEXT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"

namespace blink {

// LayoutNGRubyText represents a ruby annotation box.
// https://drafts.csswg.org/css-ruby-1/#ruby-annotation-box
class CORE_EXPORT LayoutNGRubyText final : public LayoutNGBlockFlow {
 public:
  explicit LayoutNGRubyText(Element* element);
  ~LayoutNGRubyText() override;

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutNGRubyText";
  }
  bool IsOfType(LayoutObjectType type) const override;
  bool IsChildAllowed(LayoutObject*, const ComputedStyle&) const override;
  void StyleDidChange(StyleDifference diff,
                      const ComputedStyle* old_style) override;
  bool CreatesNewFormattingContext() const override;
};

template <>
struct DowncastTraits<LayoutNGRubyText> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsRubyText();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_RUBY_TEXT_H_
