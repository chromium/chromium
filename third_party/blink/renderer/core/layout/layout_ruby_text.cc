// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_ruby_text.h"

#include "third_party/blink/renderer/core/frame/web_feature.h"

namespace blink {

LayoutRubyText::LayoutRubyText(Element* element) : LayoutBlockFlow(element) {
  DCHECK(!RuntimeEnabledFeatures::RubyLineBreakableEnabled());
}

LayoutRubyText::~LayoutRubyText() = default;

bool LayoutRubyText::CreatesNewFormattingContext() const {
  NOT_DESTROYED();
  // Ruby text objects are pushed around after layout, to become flush with
  // the associated ruby base. As such, we cannot let floats leak out from
  // ruby text objects.
  return true;
}

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

}  // namespace blink
