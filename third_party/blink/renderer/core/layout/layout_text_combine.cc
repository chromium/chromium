// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_text_combine.h"

#include "third_party/blink/renderer/core/css/resolver/style_adjuster.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_rect.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/ink_overflow.h"
#include "third_party/blink/renderer/core/layout/inline/fragment_item.h"
#include "third_party/blink/renderer/core/layout/inline/inline_node_data.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/inline_paint_context.h"
#include "third_party/blink/renderer/core/paint/line_relative_rect.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"

namespace blink {

LayoutTextCombine::LayoutTextCombine() : LayoutBlockFlow(nullptr) {
  SetIsAtomicInlineLevel(true);
}

LayoutTextCombine::~LayoutTextCombine() = default;

// static
LayoutTextCombine* LayoutTextCombine::CreateAnonymous(LayoutText* text_child) {
  DCHECK(ShouldBeParentOf(*text_child)) << text_child;
  auto* const layout_object = MakeGarbageCollected<LayoutTextCombine>();
  auto& document = text_child->GetDocument();
  layout_object->SetDocumentForAnonymous(&document);
  ComputedStyleBuilder new_style_builder =
      document.GetStyleResolver().CreateAnonymousStyleBuilderWithDisplay(
          text_child->StyleRef(), EDisplay::kInlineBlock);
  StyleAdjuster::AdjustStyleForTextCombine(new_style_builder);
  layout_object->SetStyle(new_style_builder.TakeStyle());
  layout_object->AddChild(text_child);
  LayoutTextCombine::AssertStyleIsValid(text_child->StyleRef());
  return layout_object;
}

String LayoutTextCombine::GetTextContent() const {
  DCHECK(!NeedsCollectInlines() && GetInlineNodeData()) << this;
  return GetInlineNodeData()->ItemsData(false).text_content;
}

// static
void LayoutTextCombine::AssertStyleIsValid(const ComputedStyle& style) {
  // See also |StyleAdjuster::AdjustStyleForTextCombine()|.
#if DCHECK_IS_ON()
  DCHECK_EQ(style.GetTextDecorationLine(), TextDecorationLine::kNone);
  DCHECK_EQ(style.GetTextEmphasisMark(), TextEmphasisMark::kNone);
  DCHECK_EQ(style.GetWritingMode(), WritingMode::kHorizontalTb);
  DCHECK_EQ(style.LetterSpacing(), 0.0f);
  DCHECK(!style.HasAppliedTextDecorations());
  DCHECK_EQ(style.TextIndent(), Length::Fixed());
  DCHECK_EQ(style.GetFont().GetFontDescription().Orientation(),
            FontOrientation::kHorizontal);
#endif
}

float LayoutTextCombine::DesiredWidth() const {
  DCHECK_EQ(StyleRef().GetFont().GetFontDescription().Orientation(),
            FontOrientation::kHorizontal);
  const float one_em = StyleRef().ComputedFontSize();
  if (EnumHasFlags(
          Parent()->StyleRef().TextDecorationsInEffect(),
          TextDecorationLine::kUnderline | TextDecorationLine::kOverline)) {
    return one_em;
  }
  // Allow em + 10% margin if there are no underline and overeline for
  // better looking. This isn't specified in the spec[1], but EPUB group
  // wants this.
  // [1] https://www.w3.org/TR/css-writing-modes-3/
  constexpr float kTextCombineMargin = 1.1f;
  return one_em * kTextCombineMargin;
}

float LayoutTextCombine::ComputeInlineSpacing() const {
  DCHECK_EQ(StyleRef().GetFont().GetFontDescription().Orientation(),
            FontOrientation::kHorizontal);
  DCHECK(scale_x_);
  const LayoutUnit line_height = StyleRef().GetFontHeight().LineHeight();
  return (line_height - DesiredWidth()) / 2;
}

PhysicalOffset LayoutTextCombine::ApplyScaleX(
    const PhysicalOffset& offset) const {
  DCHECK(scale_x_.has_value());
  const float spacing = ComputeInlineSpacing();
  return PhysicalOffset(LayoutUnit(offset.left * *scale_x_ + spacing),
                        offset.top);
}

PhysicalRect LayoutTextCombine::ApplyScaleX(const PhysicalRect& rect) const {
  DCHECK(scale_x_.has_value());
  return PhysicalRect(ApplyScaleX(rect.offset), ApplyScaleX(rect.size));
}

PhysicalSize LayoutTextCombine::ApplyScaleX(const PhysicalSize& size) const {
  DCHECK(scale_x_.has_value());
  return PhysicalSize(LayoutUnit(size.width * *scale_x_), size.height);
}

PhysicalOffset LayoutTextCombine::UnapplyScaleX(
    const PhysicalOffset& offset) const {
  DCHECK(scale_x_.has_value());
  const float spacing = ComputeInlineSpacing();
  return PhysicalOffset(LayoutUnit((offset.left - spacing) / *scale_x_),
                        offset.top);
}

PhysicalOffset LayoutTextCombine::AdjustOffsetForHitTest(
    const PhysicalOffset& offset_in_container) const {
  if (!scale_x_) {
    return offset_in_container;
  }
  return UnapplyScaleX(offset_in_container);
}

PhysicalOffset LayoutTextCombine::AdjustOffsetForLocalCaretRect(
    const PhysicalOffset& offset_in_container) const {
  if (!scale_x_) {
    return offset_in_container;
  }
  return ApplyScaleX(offset_in_container);
}

PhysicalRect LayoutTextCombine::AdjustRectForBoundingBox(
    const PhysicalRect& rect) const {
  if (!scale_x_) {
    return rect;
  }
  // See "text-combine-upright-compression-007.html"
  return ApplyScaleX(rect);
}

PhysicalRect LayoutTextCombine::ComputeTextBoundsRectForHitTest(
    const FragmentItem& text_item,
    const PhysicalOffset& inline_root_offset) const {
  DCHECK(text_item.IsText()) << text_item;
  PhysicalRect rect = text_item.SelfInkOverflowRect();
  rect.Move(text_item.OffsetInContainerFragment());
  rect = AdjustRectForBoundingBox(rect);
  rect.Move(inline_root_offset);
  return rect;
}

void LayoutTextCombine::ResetLayout() {
  compressed_font_ = Font();
  has_compressed_font_ = false;
  scale_x_.reset();
}

LayoutUnit LayoutTextCombine::AdjustTextLeftForPaint(
    LayoutUnit position) const {
  if (!scale_x_) {
    return position;
  }
  const float spacing = ComputeInlineSpacing();
  return LayoutUnit(position + spacing / *scale_x_);
}

LayoutUnit LayoutTextCombine::AdjustTextTopForPaint(LayoutUnit text_top) const {
  DCHECK_EQ(StyleRef().GetFont().GetFontDescription().Orientation(),
            FontOrientation::kHorizontal);
  const SimpleFontData& font_data = *StyleRef().GetFont().PrimaryFont();
  const float internal_leading = font_data.InternalLeading();
  const float half_leading = internal_leading / 2;
  const int ascent = font_data.GetFontMetrics().Ascent();
  return LayoutUnit(text_top + ascent - half_leading);
}

AffineTransform LayoutTextCombine::ComputeAffineTransformForPaint(
    const PhysicalOffset& paint_offset) const {
  DCHECK(NeedsAffineTransformInPaint());
  AffineTransform matrix;
  if (UsingSyntheticOblique()) {
    const LayoutUnit text_left = AdjustTextLeftForPaint(paint_offset.left);
    const LayoutUnit text_top = AdjustTextTopForPaint(paint_offset.top);
    matrix.Translate(text_left, text_top);
    // TODO(yosin): We should use angle specified in CSS instead of
    // constant value -15deg. See also |DrawBlobs()| in [1] for vertical
    // upright oblique.
    // [1] "third_party/blink/renderer/platform/fonts/font.cc"
    constexpr float kSlantAngle = -15.0f;
    matrix.SkewY(kSlantAngle);
    matrix.Translate(-text_left, -text_top);
  }
  if (scale_x_.has_value()) {
    matrix.Translate(paint_offset.left, paint_offset.top);
    matrix.Scale(*scale_x_, 1.0f);
    matrix.Translate(-paint_offset.left, -paint_offset.top);
  }
  return matrix;
}

bool LayoutTextCombine::NeedsAffineTransformInPaint() const {
  return scale_x_.has_value() || UsingSyntheticOblique();
}

LineRelativeRect LayoutTextCombine::ComputeTextFrameRect(
    const PhysicalOffset paint_offset) const {
  const ComputedStyle& style = Parent()->StyleRef();
  DCHECK(style.GetFont().GetFontDescription().IsVerticalBaseline());

  const LayoutUnit one_em = style.ComputedFontSizeAsFixed();
  const FontHeight text_metrics = style.GetFontHeight();
  const LayoutUnit line_height = text_metrics.LineHeight();
  return {LineRelativeOffset::CreateFromBoxOrigin(paint_offset),
          LogicalSize(one_em, line_height)};
}

PhysicalRect LayoutTextCombine::RecalcContentsInkOverflow(
    const InlineCursor& cursor) const {
  const ComputedStyle& style = Parent()->StyleRef();
  DCHECK(style.GetFont().GetFontDescription().IsVerticalBaseline());

  const LineRelativeRect line_relative_text_rect =
      ComputeTextFrameRect(PhysicalOffset());

  // Note: |text_rect| and |ink_overflow| are both in logical direction.
  // It is unusual for a PhysicalRect to be in a logical direction, typically
  // a LineRelativeRect will be used instead, but the TextCombine case
  // requires it.
  const PhysicalRect text_rect{
      PhysicalOffset(), PhysicalSize{line_relative_text_rect.size.inline_size,
                                     line_relative_text_rect.size.block_size}};
  LogicalRect ink_overflow(text_rect.offset.left, text_rect.offset.top,
                           text_rect.size.width, text_rect.size.height);

  const WritingMode writing_mode = style.GetWritingMode();
  if (style.HasAppliedTextDecorations()) {
    // |LayoutTextCombine| does not support decorating box, as it is not
    // supported in vertical flow and text-combine is only for vertical flow.
    const LogicalRect decoration_rect = InkOverflow::ComputeDecorationOverflow(
        cursor, style, style.GetFont(),
        /* offset_in_container */ PhysicalOffset(), ink_overflow,
        /* inline_context */ nullptr, writing_mode);
    ink_overflow.Unite(decoration_rect);
  }

  if (style.GetTextEmphasisMark() != TextEmphasisMark::kNone) {
    ink_overflow = InkOverflow::ComputeEmphasisMarkOverflow(
        style, text_rect.size, ink_overflow);
  }

  if (const ShadowList* text_shadow = style.TextShadow()) {
    InkOverflow::ExpandForShadowOverflow(ink_overflow, *text_shadow,
                                         writing_mode);
  }

  PhysicalRect local_ink_overflow =
      WritingModeConverter({writing_mode, TextDirection::kLtr}, text_rect.size)
          .ToPhysical(ink_overflow);
  local_ink_overflow.ExpandEdgesToPixelBoundaries();
  return local_ink_overflow;
}

gfx::Rect LayoutTextCombine::VisualRectForPaint(
    const PhysicalOffset& paint_offset) const {
  DCHECK_EQ(PhysicalFragmentCount(), 1u);
  PhysicalRect ink_overflow = GetPhysicalFragment(0)->InkOverflowRect();
  ink_overflow.Move(paint_offset);
  return ToEnclosingRect(ink_overflow);
}

void LayoutTextCombine::SetScaleX(float new_scale_x) {
  DCHECK_GT(new_scale_x, 0.0f);
  DCHECK(!scale_x_.has_value());
  DCHECK(!has_compressed_font_);
  // Note: Even if rounding, e.g. LayoutUnit::FromFloatRound(), we still have
  // gap between painted characters in text-combine-upright-value-all-002.html
  scale_x_ = new_scale_x;
}

void LayoutTextCombine::SetCompressedFont(const Font& font) {
  DCHECK(!has_compressed_font_);
  DCHECK(!scale_x_.has_value());
  compressed_font_ = font;
  has_compressed_font_ = true;
}

bool LayoutTextCombine::UsingSyntheticOblique() const {
  return Parent()
      ->StyleRef()
      .GetFont()
      .GetFontDescription()
      .IsSyntheticOblique();
}

}  // namespace blink
