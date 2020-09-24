/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_RUBY_TEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_RUBY_TEXT_H_

#include "third_party/blink/renderer/core/layout/layout_block_flow.h"

namespace blink {

class LayoutRubyText : public LayoutBlockFlow {
 public:
  LayoutRubyText(Element*);
  ~LayoutRubyText() override;

  const char* GetName() const override { return "LayoutRubyText"; }

  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectRubyText || LayoutBlockFlow::IsOfType(type);
  }

  bool IsChildAllowed(LayoutObject*, const ComputedStyle&) const override;

  void StyleDidChange(StyleDifference diff,
                      const ComputedStyle* old_style) override;

  bool CreatesNewFormattingContext() const final {
    // Ruby text objects are pushed around after layout, to become flush with
    // the associated ruby base. As such, we cannot let floats leak out from
    // ruby text objects.
    return true;
  }

 private:
  ETextAlign TextAlignmentForLine(bool ends_with_soft_break) const override;
  void AdjustInlineDirectionLineBounds(
      unsigned expansion_opportunity_count,
      LayoutUnit& logical_left,
      LayoutUnit& logical_width) const override;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutRubyText, IsRubyText());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_RUBY_TEXT_H_
