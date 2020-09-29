// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_ink_overflow.h"

#include "third_party/blink/renderer/core/layout/geometry/logical_rect.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/line/line_orientation_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_text_decoration_offset.h"
#include "third_party/blink/renderer/core/paint/text_decoration_info.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

namespace {

struct SameSizeAsNGInkOverflow {
  void* pointer;
#if DCHECK_IS_ON()
  NGInkOverflow::Type type;
#endif
};

ASSERT_SIZE(NGInkOverflow, SameSizeAsNGInkOverflow);

inline bool HasOverflow(const PhysicalRect& rect, const PhysicalSize& size) {
  if (rect.IsEmpty())
    return false;
  return rect.X() < 0 || rect.Y() < 0 || rect.Right() > size.width ||
         rect.Bottom() > size.height;
}

}  // namespace

#if DCHECK_IS_ON()
unsigned NGInkOverflow::read_unset_as_none_ = 0;

NGInkOverflow::~NGInkOverflow() {
  // Because |Type| is kept outside of the instance, callers must call |Reset|
  // before destructing.
  DCHECK(type_ == kNotSet || type_ == kNone) << type_;
}
#endif

NGInkOverflow::NGInkOverflow(Type source_type, const NGInkOverflow& source) {
  source.CheckType(source_type);
  new (this) NGInkOverflow();
  switch (source_type) {
    case kNotSet:
    case kNone:
      break;
    case kSmallSelf:
    case kSmallContents:
      static_assert(sizeof(outsets_) == sizeof(single_),
                    "outsets should be the size of a pointer");
      single_ = source.single_;
#if DCHECK_IS_ON()
      for (wtf_size_t i = 0; i < base::size(outsets_); ++i)
        DCHECK_EQ(outsets_[i], source.outsets_[i]);
#endif
      break;
    case kSelf:
    case kContents:
      single_ = new NGSingleInkOverflow(*source.single_);
      break;
    case kSelfAndContents:
      container_ = new NGContainerInkOverflow(*source.container_);
      break;
  }
  SetType(source_type);
}

NGInkOverflow::NGInkOverflow(Type source_type, NGInkOverflow&& source) {
  source.CheckType(source_type);
  new (this) NGInkOverflow();
  switch (source_type) {
    case kNotSet:
    case kNone:
      break;
    case kSmallSelf:
    case kSmallContents:
      static_assert(sizeof(outsets_) == sizeof(single_),
                    "outsets should be the size of a pointer");
      single_ = source.single_;
#if DCHECK_IS_ON()
      for (wtf_size_t i = 0; i < base::size(outsets_); ++i)
        DCHECK_EQ(outsets_[i], source.outsets_[i]);
#endif
      break;
    case kSelf:
    case kContents:
      single_ = source.single_;
      source.single_ = nullptr;
      break;
    case kSelfAndContents:
      container_ = source.container_;
      source.container_ = nullptr;
      break;
  }
  SetType(source_type);
}

NGInkOverflow::Type NGInkOverflow::Reset(Type type, Type new_type) {
  CheckType(type);
  DCHECK(new_type == kNotSet || new_type == kNone);
  switch (type) {
    case kNotSet:
    case kNone:
    case kSmallSelf:
    case kSmallContents:
      break;
    case kSelf:
    case kContents:
      delete single_;
      break;
    case kSelfAndContents:
      delete container_;
      break;
  }
  return SetType(new_type);
}

PhysicalRect NGInkOverflow::FromOutsets(const PhysicalSize& size) const {
  const LayoutUnit left_outset(LayoutUnit::FromRawValue(outsets_[0]));
  const LayoutUnit top_outset(LayoutUnit::FromRawValue(outsets_[1]));
  return {-left_outset, -top_outset,
          left_outset + size.width + LayoutUnit::FromRawValue(outsets_[2]),
          top_outset + size.height + LayoutUnit::FromRawValue(outsets_[3])};
}

PhysicalRect NGInkOverflow::Self(Type type, const PhysicalSize& size) const {
  CheckType(type);
  DCHECK_NE(type, kNotSet);
  switch (type) {
    case kNotSet:
    case kNone:
    case kSmallContents:
    case kContents:
      return {PhysicalOffset(), size};
    case kSmallSelf:
      return FromOutsets(size);
    case kSelf:
    case kSelfAndContents:
      DCHECK(single_);
      return single_->ink_overflow;
  }
  NOTREACHED();
  return {PhysicalOffset(), size};
}

PhysicalRect NGInkOverflow::SelfAndContents(Type type,
                                            const PhysicalSize& size) const {
  CheckType(type);
  switch (type) {
    case kNotSet:
#if DCHECK_IS_ON()
      if (!read_unset_as_none_)
        NOTREACHED();
      FALLTHROUGH;
#endif
    case kNone:
      return {PhysicalOffset(), size};
    case kSmallSelf:
    case kSmallContents:
      return FromOutsets(size);
    case kSelf:
    case kContents:
      DCHECK(single_);
      return single_->ink_overflow;
    case kSelfAndContents:
      DCHECK(container_);
      return container_->SelfAndContentsInkOverflow();
  }
  NOTREACHED();
  return {PhysicalOffset(), size};
}

// Store |ink_overflow| as |SmallRawValue| if possible and returns |true|.
// Returns |false| if |ink_overflow| is too large for |SmallRawValue|.
bool NGInkOverflow::TrySetOutsets(Type type,
                                  LayoutUnit left_outset,
                                  LayoutUnit top_outset,
                                  LayoutUnit right_outset,
                                  LayoutUnit bottom_outset) {
  CheckType(type);
  const LayoutUnit max_small_value(
      LayoutUnit::FromRawValue(std::numeric_limits<SmallRawValue>::max()));
  if (left_outset > max_small_value)
    return false;
  if (top_outset > max_small_value)
    return false;
  if (right_outset > max_small_value)
    return false;
  if (bottom_outset > max_small_value)
    return false;
  Reset(type);
  outsets_[0] = left_outset.RawValue();
  outsets_[1] = top_outset.RawValue();
  outsets_[2] = right_outset.RawValue();
  outsets_[3] = bottom_outset.RawValue();
  return true;
}

NGInkOverflow::Type NGInkOverflow::SetSingle(Type type,
                                             const PhysicalRect& ink_overflow,
                                             const PhysicalSize& size,
                                             Type new_type,
                                             Type new_small_type) {
  CheckType(type);
  DCHECK(HasOverflow(ink_overflow, size));

  const LayoutUnit left_outset = (-ink_overflow.X()).ClampNegativeToZero();
  const LayoutUnit top_outset = (-ink_overflow.Y()).ClampNegativeToZero();
  const LayoutUnit right_outset =
      (ink_overflow.Right() - size.width).ClampNegativeToZero();
  const LayoutUnit bottom_outset =
      (ink_overflow.Bottom() - size.height).ClampNegativeToZero();

  if (TrySetOutsets(type, left_outset, top_outset, right_outset, bottom_outset))
    return SetType(new_small_type);

  const PhysicalRect adjusted_ink_overflow(
      -left_outset, -top_outset, left_outset + size.width + right_outset,
      top_outset + size.height + bottom_outset);

  switch (type) {
    case kSelfAndContents:
      Reset(type);
      FALLTHROUGH;
    case kNotSet:
    case kNone:
    case kSmallSelf:
    case kSmallContents:
      single_ = new NGSingleInkOverflow(adjusted_ink_overflow);
      return SetType(new_type);
    case kSelf:
    case kContents:
      DCHECK(single_);
      single_->ink_overflow = adjusted_ink_overflow;
      return SetType(new_type);
  }
  NOTREACHED();
}

NGInkOverflow::Type NGInkOverflow::SetSelf(Type type,
                                           const PhysicalRect& ink_overflow,
                                           const PhysicalSize& size) {
  CheckType(type);
  if (!HasOverflow(ink_overflow, size))
    return Reset(type);
  return SetSingle(type, ink_overflow, size, kSelf, kSmallSelf);
}

NGInkOverflow::Type NGInkOverflow::SetContents(Type type,
                                               const PhysicalRect& ink_overflow,
                                               const PhysicalSize& size) {
  CheckType(type);
  if (!HasOverflow(ink_overflow, size))
    return Reset(type);
  return SetSingle(type, ink_overflow, size, kContents, kSmallContents);
}

NGInkOverflow::Type NGInkOverflow::Set(Type type,
                                       const PhysicalRect& self,
                                       const PhysicalRect& contents,
                                       const PhysicalSize& size) {
  CheckType(type);

  if (!HasOverflow(self, size)) {
    if (!HasOverflow(contents, size))
      return Reset(type);
    return SetSingle(type, contents, size, kContents, kSmallContents);
  }
  if (!HasOverflow(contents, size))
    return SetSingle(type, self, size, kSelf, kSmallSelf);

  switch (type) {
    case kSelf:
    case kContents:
      Reset(type);
      FALLTHROUGH;
    case kNotSet:
    case kNone:
    case kSmallSelf:
    case kSmallContents:
      container_ = new NGContainerInkOverflow(self, contents);
      return SetType(kSelfAndContents);
    case kSelfAndContents:
      DCHECK(container_);
      container_->ink_overflow = self;
      container_->contents_ink_overflow = contents;
      return kSelfAndContents;
  }
  NOTREACHED();
}

NGInkOverflow::Type NGInkOverflow::SetTextInkOverflow(
    Type type,
    const NGTextFragmentPaintInfo& text_info,
    const ComputedStyle& style,
    const PhysicalSize& size,
    PhysicalRect* ink_overflow_out) {
  CheckType(type);
  DCHECK_EQ(type, kNotSet);
  base::Optional<PhysicalRect> ink_overflow =
      ComputeTextInkOverflow(text_info, style, size);
  if (!ink_overflow) {
    *ink_overflow_out = {PhysicalOffset(), size};
    return Reset(type);
  }
  *ink_overflow_out = *ink_overflow;
  return SetSelf(type, *ink_overflow, size);
}

// static
base::Optional<PhysicalRect> NGInkOverflow::ComputeTextInkOverflow(
    const NGTextFragmentPaintInfo& text_info,
    const ComputedStyle& style,
    const PhysicalSize& size) {
  // Glyph bounds is in logical coordinate, origin at the alphabetic baseline.
  const Font& font = style.GetFont();
  const FloatRect text_ink_bounds = font.TextInkBounds(text_info);
  LayoutRect ink_overflow = EnclosingLayoutRect(text_ink_bounds);

  // Make the origin at the logical top of this fragment.
  if (const SimpleFontData* font_data = font.PrimaryFont()) {
    ink_overflow.SetY(
        ink_overflow.Y() +
        font_data->GetFontMetrics().FixedAscent(kAlphabeticBaseline));
  }

  if (float stroke_width = style.TextStrokeWidth()) {
    ink_overflow.Inflate(LayoutUnit::FromFloatCeil(stroke_width / 2.0f));
  }

  // Following effects, such as shadows, operate on the text decorations,
  // so compute text decoration overflow first.
  if (!style.AppliedTextDecorations().IsEmpty() && font.PrimaryFont()) {
    LayoutRect decoration_rect =
        ComputeTextDecorationOverflow(text_info, style, ink_overflow);
    ink_overflow.Unite(decoration_rect);
  }

  const WritingMode writing_mode = style.GetWritingMode();
  if (style.GetTextEmphasisMark() != TextEmphasisMark::kNone) {
    LayoutUnit emphasis_mark_height =
        LayoutUnit(font.EmphasisMarkHeight(style.TextEmphasisMarkString()));
    DCHECK_GE(emphasis_mark_height, LayoutUnit());
    if (style.GetTextEmphasisLineLogicalSide() == LineLogicalSide::kOver) {
      ink_overflow.ShiftYEdgeTo(
          std::min(ink_overflow.Y(), -emphasis_mark_height));
    } else {
      LayoutUnit logical_height =
          IsHorizontalWritingMode(writing_mode) ? size.height : size.width;
      ink_overflow.ShiftMaxYEdgeTo(
          std::max(ink_overflow.MaxY(), logical_height + emphasis_mark_height));
    }
  }

  if (ShadowList* text_shadow = style.TextShadow()) {
    LayoutRectOutsets text_shadow_logical_outsets =
        LineOrientationLayoutRectOutsets(
            LayoutRectOutsets(text_shadow->RectOutsetsIncludingOriginal()),
            writing_mode);
    text_shadow_logical_outsets.ClampNegativeToZero();
    ink_overflow.Expand(text_shadow_logical_outsets);
  }

  PhysicalRect local_ink_overflow =
      WritingModeConverter({writing_mode, TextDirection::kLtr}, size)
          .ToPhysical(LogicalRect(ink_overflow));

  // Uniting the frame rect ensures that non-ink spaces such side bearings, or
  // even space characters, are included in the visual rect for decorations.
  if (!HasOverflow(local_ink_overflow, size))
    return base::nullopt;

  local_ink_overflow.Unite({{}, size});
  local_ink_overflow.ExpandEdgesToPixelBoundaries();
  return local_ink_overflow;
}

LayoutRect NGInkOverflow::ComputeTextDecorationOverflow(
    const NGTextFragmentPaintInfo& text_info,
    const ComputedStyle& style,
    const LayoutRect& ink_overflow) {
  // Use a zero offset because all offsets are applied to the ink overflow
  // after it has been computed.
  PhysicalOffset offset;
  TextDecorationInfo decoration_info(offset, offset, ink_overflow.Width(),
                                     style.GetFontBaseline(), style, nullptr);
  NGTextDecorationOffset decoration_offset(decoration_info.Style(), style,
                                           nullptr);
  const Vector<AppliedTextDecoration>& decorations =
      style.AppliedTextDecorations();

  // text-underline-position may flip underline and overline.
  ResolvedUnderlinePosition underline_position =
      decoration_info.UnderlinePosition();
  bool flip_underline_and_overline = false;
  if (underline_position == ResolvedUnderlinePosition::kOver) {
    flip_underline_and_overline = true;
    underline_position = ResolvedUnderlinePosition::kUnder;
  }

  FloatRect accumulated_bound;
  for (size_t applied_decoration_index = 0;
       applied_decoration_index < decorations.size();
       ++applied_decoration_index) {
    const AppliedTextDecoration& decoration =
        decorations[applied_decoration_index];
    TextDecoration lines = decoration.Lines();
    bool has_underline = EnumHasFlags(lines, TextDecoration::kUnderline);
    bool has_overline = EnumHasFlags(lines, TextDecoration::kOverline);
    if (flip_underline_and_overline)
      std::swap(has_underline, has_overline);

    decoration_info.SetDecorationIndex(applied_decoration_index);

    float resolved_thickness = decoration_info.ResolvedThickness();

    if (has_underline) {
      const int paint_underline_offset =
          decoration_offset.ComputeUnderlineOffset(
              underline_position, decoration_info.Style().ComputedFontSize(),
              decoration_info.FontData()->GetFontMetrics(),
              decoration.UnderlineOffset(), resolved_thickness);
      decoration_info.SetPerLineData(
          TextDecoration::kUnderline, paint_underline_offset,
          TextDecorationInfo::DoubleOffsetFromThickness(resolved_thickness), 1);
      accumulated_bound.Unite(
          decoration_info.BoundsForLine(TextDecoration::kUnderline));
    }
    if (has_overline) {
      FontVerticalPositionType position =
          flip_underline_and_overline ? FontVerticalPositionType::TopOfEmHeight
                                      : FontVerticalPositionType::TextTop;
      const int paint_overline_offset =
          decoration_offset.ComputeUnderlineOffsetForUnder(
              decoration_info.Style().TextUnderlineOffset(),
              decoration_info.Style().ComputedFontSize(), resolved_thickness,
              position);
      decoration_info.SetPerLineData(
          TextDecoration::kOverline, paint_overline_offset,
          -TextDecorationInfo::DoubleOffsetFromThickness(resolved_thickness),
          1);
      accumulated_bound.Unite(
          decoration_info.BoundsForLine(TextDecoration::kOverline));
    }
    if (EnumHasFlags(lines, TextDecoration::kLineThrough)) {
      // For increased line thickness, the line-through decoration needs to grow
      // in both directions from its origin, subtract half the thickness to keep
      // it centered at the same origin.
      const float line_through_offset =
          2 * decoration_info.Baseline() / 3 - resolved_thickness / 2;
      // Floor double_offset in order to avoid double-line gap to appear
      // of different size depending on position where the double line
      // is drawn because of rounding downstream in
      // GraphicsContext::DrawLineForText.
      decoration_info.SetPerLineData(
          TextDecoration::kLineThrough, line_through_offset,
          floorf(TextDecorationInfo::DoubleOffsetFromThickness(
              resolved_thickness)),
          0);
      accumulated_bound.Unite(
          decoration_info.BoundsForLine(TextDecoration::kLineThrough));
    }
  }
  return EnclosingLayoutRect(accumulated_bound);
}

}  // namespace blink
