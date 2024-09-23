// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_RUBY_TEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_RUBY_TEXT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"

namespace blink {

// LayoutRubyText represents a ruby annotation box.
// https://drafts.csswg.org/css-ruby-1/#ruby-annotation-box
class CORE_EXPORT LayoutRubyText final : public LayoutBlockFlow {
 public:
  explicit LayoutRubyText(Element* element);
  ~LayoutRubyText() override;

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutRubyText";
  }
  bool IsRubyText() const final {
    NOT_DESTROYED();
    return true;
  }
  bool IsChildAllowed(LayoutObject*, const ComputedStyle&) const override;
  void StyleDidChange(StyleDifference diff,
                      const ComputedStyle* old_style) override;
  bool CreatesNewFormattingContext() const override;
};

template <>
struct DowncastTraits<LayoutRubyText> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsRubyText();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_RUBY_TEXT_H_
