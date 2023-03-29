// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/layout_ng_ruby_text.h"

#include "third_party/blink/renderer/core/frame/web_feature.h"

namespace blink {

LayoutNGRubyText::LayoutNGRubyText(Element* element)
    : LayoutNGBlockFlow(element) {}

LayoutNGRubyText::~LayoutNGRubyText() = default;

bool LayoutNGRubyText::IsOfType(LayoutObjectType type) const {
  NOT_DESTROYED();
  return type == kLayoutObjectRubyText || LayoutNGBlockFlow::IsOfType(type);
}

bool LayoutNGRubyText::CreatesNewFormattingContext() const {
  NOT_DESTROYED();
  // Ruby text objects are pushed around after layout, to become flush with
  // the associated ruby base. As such, we cannot let floats leak out from
  // ruby text objects.
  return true;
}

bool LayoutNGRubyText::IsChildAllowed(LayoutObject* child,
                                      const ComputedStyle&) const {
  NOT_DESTROYED();
  return child->IsInline();
}

void LayoutNGRubyText::StyleDidChange(StyleDifference diff,
                                      const ComputedStyle* old_style) {
  NOT_DESTROYED();
  if (StyleRef().GetTextAlign() !=
      ComputedStyleInitialValues::InitialTextAlign()) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kRubyTextWithNonDefaultTextAlign);
  }
  LayoutNGBlockFlow::StyleDidChange(diff, old_style);
}

}  // namespace blink
