/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/layout/layout_ruby_text.h"

#include "third_party/blink/renderer/core/frame/web_feature.h"

namespace blink {

LayoutRubyText::LayoutRubyText(ContainerNode* node) : LayoutBlockFlow(node) {}

LayoutRubyText::~LayoutRubyText() = default;

bool LayoutRubyText::IsChildAllowed(LayoutObject* child,
                                    const ComputedStyle&) const {
  NOT_DESTROYED();
  return child->IsInline();
}

void LayoutRubyText::StyleDidChange(StyleDifference diff,
                                    const ComputedStyle* old_style) {
  NOT_DESTROYED();
  if (StyleRef().GetTextAlign() !=
      ComputedStyleInitialValues::InitialTextAlign()) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kRubyTextWithNonDefaultTextAlign);
  }
  LayoutBlockFlow::StyleDidChange(diff, old_style);
}

ETextAlign LayoutRubyText::TextAlignmentForLine(
    bool ends_with_soft_break) const {
  NOT_DESTROYED();
  ETextAlign text_align = StyleRef().GetTextAlign();
  // FIXME: This check is bogus since user can set the initial value.
  if (text_align != ComputedStyleInitialValues::InitialTextAlign())
    return LayoutBlockFlow::TextAlignmentForLine(ends_with_soft_break);

  // The default behavior is to allow ruby text to expand if it is shorter than
  // the ruby base.
  return ETextAlign::kJustify;
}

void LayoutRubyText::AdjustInlineDirectionLineBounds(
    unsigned expansion_opportunity_count,
    LayoutUnit& logical_left,
    LayoutUnit& logical_width) const {
  NOT_DESTROYED();
  ETextAlign text_align = StyleRef().GetTextAlign();
  // FIXME: This check is bogus since user can set the initial value.
  if (text_align != ComputedStyleInitialValues::InitialTextAlign()) {
    return LayoutBlockFlow::AdjustInlineDirectionLineBounds(
        expansion_opportunity_count, logical_left, logical_width);
  }

  int max_preferred_logical_width = PreferredLogicalWidths().max_size.ToInt();
  if (max_preferred_logical_width >= logical_width)
    return;

  // Inset the ruby text by half the inter-ideograph expansion amount, but no
  // more than a full-width ruby character on each side.
  LayoutUnit inset = (logical_width - max_preferred_logical_width) /
                     (expansion_opportunity_count + 1);
  if (expansion_opportunity_count)
    inset = std::min(LayoutUnit(2 * StyleRef().FontSize()), inset);

  logical_left += inset / 2;
  logical_width -= inset;
}

}  // namespace blink
