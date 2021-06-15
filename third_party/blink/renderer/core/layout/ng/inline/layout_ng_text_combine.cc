// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/layout_ng_text_combine.h"

#include "third_party/blink/renderer/core/css/resolver/style_adjuster.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_rect.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node_data.h"
#include "third_party/blink/renderer/core/layout/ng/ng_ink_overflow.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
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

String LayoutNGTextCombine::GetTextContent() const {
  DCHECK(!NeedsCollectInlines() && HasNGInlineNodeData()) << this;
  return GetNGInlineNodeData()->ItemsData(false).text_content;
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

float LayoutNGTextCombine::DesiredWidth() const {
  DCHECK_EQ(StyleRef().GetFont().GetFontDescription().Orientation(),
            FontOrientation::kHorizontal);
  const float one_em = StyleRef().ComputedFontSize();
  if (EnumHasFlags(Parent()->StyleRef().TextDecorationsInEffect(),
                   TextDecoration::kUnderline | TextDecoration::kOverline))
    return one_em;
  // Allow em + 10% margin if there are no underline and overeline for
  // better looking. This isn't specified in the spec[1], but EPUB group
  // wants this.
  // [1] https://www.w3.org/TR/css-writing-modes-3/
  constexpr float kTextCombineMargin = 1.1f;
  return one_em * kTextCombineMargin;
}

void LayoutNGTextCombine::ResetLayout() {
  compressed_font_.reset();
  scale_x_.reset();
}

void LayoutNGTextCombine::SetScaleX(float new_scale_x) {
  DCHECK_GT(new_scale_x, 0.0f);
  DCHECK(!scale_x_.has_value());
  DCHECK(!compressed_font_.has_value());
  // Note: Even if rounding, e.g. LayoutUnit::FromFloatRound(), we still have
  // gap between painted characters in text-combine-upright-value-all-002.html
  scale_x_ = new_scale_x;
}

void LayoutNGTextCombine::SetCompressedFont(const Font& font) {
  DCHECK(!compressed_font_.has_value());
  DCHECK(!scale_x_.has_value());
  compressed_font_ = font;
}

}  // namespace blink
