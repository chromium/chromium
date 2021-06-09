// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/layout_ng_text_combine.h"

#include "third_party/blink/renderer/core/css/resolver/style_adjuster.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node_data.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"

namespace blink {

LayoutNGTextCombine::LayoutNGTextCombine() : LayoutNGBlockFlow(nullptr) {
  SetIsAtomicInlineLevel(true);
}

LayoutNGTextCombine::~LayoutNGTextCombine() = default;

// static
LayoutNGTextCombine* LayoutNGTextCombine::CreateAnonymous(
    LayoutText* text_child) {
  DCHECK(ShouldBeParentOf(*text_child)) << text_child;
  auto* const layout_object = new LayoutNGTextCombine();
  auto& document = text_child->GetDocument();
  layout_object->SetDocumentForAnonymous(&document);
  scoped_refptr<ComputedStyle> new_style =
      document.GetStyleResolver().CreateAnonymousStyleWithDisplay(
          text_child->StyleRef(), EDisplay::kInlineBlock);
  StyleAdjuster::AdjustStyleForTextCombine(*new_style);
  layout_object->SetStyleInternal(std::move(new_style));
  layout_object->AddChild(text_child);
  LayoutNGTextCombine::AssertStyleIsValid(text_child->StyleRef());
  return layout_object;
}

// static
void LayoutNGTextCombine::AssertStyleIsValid(const ComputedStyle& style) {
  // See also |StyleAdjuster::AdjustStyleForTextCombine()|.
#if DCHECK_IS_ON()
  DCHECK_EQ(style.GetTextDecoration(), TextDecoration::kNone);
  DCHECK_EQ(style.GetTextEmphasisMark(), TextEmphasisMark::kNone);
  DCHECK_EQ(style.GetWritingMode(), WritingMode::kHorizontalTb);
  DCHECK_EQ(style.LetterSpacing(), 0.0f);
  DCHECK_EQ(style.TextDecorationsInEffect(), TextDecoration::kNone);
  DCHECK_EQ(style.GetFont().GetFontDescription().Orientation(),
            FontOrientation::kHorizontal);
#endif
}

bool LayoutNGTextCombine::IsOfType(LayoutObjectType type) const {
  NOT_DESTROYED();
  return type == kLayoutObjectNGTextCombine ||
         LayoutNGBlockFlow::IsOfType(type);
}

}  // namespace blink
