// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_ink_overflow.h"

#include "build/chromeos_buildflags.h"
#include "third_party/blink/renderer/core/editing/markers/custom_highlight_marker.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/highlight/highlight_style_utils.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_rect.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"
#include "third_party/blink/renderer/core/layout/ng/ng_text_decoration_offset.h"
#include "third_party/blink/renderer/core/paint/ng/ng_highlight_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_inline_paint_context.h"
#include "third_party/blink/renderer/core/paint/text_decoration_info.h"
#include "third_party/blink/renderer/core/style/applied_text_decoration.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
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
// Define this for the debugging purpose to DCHECK if uncomputed ink overflow is
// happening. As DCHECK builds ship, enabling this for all DCHECK builds causes
// more troubles than to help.
//
// #define DISALLOW_READING_UNSET

unsigned NGInkOverflow::read_unset_as_none_ = 0;

NGInkOverflow::~NGInkOverflow() {
  // Because |Type| is kept outside of the instance, callers must call |Reset|
  // before destructing.
  DCHECK(type_ == Type::kNotSet || type_ == Type::kNone ||
         type_ == Type::kInvalidated)
      << static_cast<int>(type_);
}
#endif

NGInkOverflow::NGInkOverflow(Type source_type, const NGInkOverflow& source) {
  source.CheckType(source_type);
  new (this) NGInkOverflow();
  switch (source_type) {
    case Type::kNotSet:
    case Type::kInvalidated:
    case Type::kNone:
      break;
    case Type::kSmallSelf:
    case Type::kSmallContents:
      static_assert(sizeof(outsets_) == sizeof(single_),
                    "outsets should be the size of a pointer");
      single_ = source.single_;
#if DCHECK_IS_ON()
      for (wtf_size_t i = 0; i < std::size(outsets_); ++i)
        DCHECK_EQ(outsets_[i], source.outsets_[i]);
#endif
      break;
    case Type::kSelf:
    case Type::kContents:
      single_ = new NGSingleInkOverflow(*source.single_);
      break;
    case Type::kSelfAndContents:
      container_ = new NGContainerInkOverflow(*source.container_);
      break;
  }
  SetType(source_type);
}

NGInkOverflow::NGInkOverflow(Type source_type, NGInkOverflow&& source) {
  source.CheckType(source_type);
  new (this) NGInkOverflow();
  switch (source_type) {
    case Type::kNotSet:
    case Type::kInvalidated:
    case Type::kNone:
      break;
    case Type::kSmallSelf:
    case Type::kSmallContents:
      static_assert(sizeof(outsets_) == sizeof(single_),
                    "outsets should be the size of a pointer");
      single_ = source.single_;
#if DCHECK_IS_ON()
      for (wtf_size_t i = 0; i < std::size(outsets_); ++i)
        DCHECK_EQ(outsets_[i], source.outsets_[i]);
#endif
      break;
    case Type::kSelf:
    case Type::kContents:
      single_ = source.single_;
      source.single_ = nullptr;
      break;
    case Type::kSelfAndContents:
      container_ = source.container_;
      source.container_ = nullptr;
      break;
  }
  SetType(source_type);
}

NGInkOverflow::Type NGInkOverflow::Reset(Type type, Type new_type) {
  CheckType(type);
  DCHECK(new_type == Type::kNotSet || new_type == Type::kNone ||
         new_type == Type::kInvalidated);
  switch (type) {
    case Type::kNotSet:
    case Type::kInvalidated:
    case Type::kNone:
    case Type::kSmallSelf:
    case Type::kSmallContents:
      break;
    case Type::kSelf:
    case Type::kContents:
      delete single_;
      break;
    case Type::kSelfAndContents:
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
  switch (type) {
    case Type::kNotSet:
    case Type::kInvalidated:
#if defined(DISALLOW_READING_UNSET)
      if (!read_unset_as_none_)
        NOTREACHED();
      [[fallthrough]];
#endif
    case Type::kNone:
    case Type::kSmallContents:
    case Type::kContents:
      return {PhysicalOffset(), size};
    case Type::kSmallSelf:
      return FromOutsets(size);
    case Type::kSelf:
    case Type::kSelfAndContents:
      DCHECK(single_);
      return single_->ink_overflow;
  }
  NOTREACHED();
  return {PhysicalOffset(), size};
}

PhysicalRect NGInkOverflow::Contents(Type type,
                                     const PhysicalSize& size) const {
  CheckType(type);
  switch (type) {
    case Type::kNotSet:
    case Type::kInvalidated:
#if defined(DISALLOW_READING_UNSET)
      if (!read_unset_as_none_)
        NOTREACHED();
      [[fallthrough]];
#endif
    case Type::kNone:
    case Type::kSmallSelf:
    case Type::kSelf:
      return PhysicalRect();
    case Type::kSmallContents:
      return FromOutsets(size);
    case Type::kContents:
      DCHECK(single_);
      return single_->ink_overflow;
    case Type::kSelfAndContents:
      DCHECK(container_);
      return container_->contents_ink_overflow;
  }
  NOTREACHED();
  return PhysicalRect();
}

PhysicalRect NGInkOverflow::SelfAndContents(Type type,
                                            const PhysicalSize& size) const {
  CheckType(type);
  switch (type) {
    case Type::kNotSet:
    case Type::kInvalidated:
#if defined(DISALLOW_READING_UNSET)
      if (!read_unset_as_none_)
        NOTREACHED();
      [[fallthrough]];
#endif
    case Type::kNone:
      return {PhysicalOffset(), size};
    case Type::kSmallSelf:
    case Type::kSmallContents:
      return FromOutsets(size);
    case Type::kSelf:
    case Type::kContents:
      DCHECK(single_);
      return single_->ink_overflow;
    case Type::kSelfAndContents:
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
    case Type::kSelfAndContents:
      Reset(type);
      [[fallthrough]];
    case Type::kNotSet:
    case Type::kInvalidated:
    case Type::kNone:
    case Type::kSmallSelf:
    case Type::kSmallContents:
      single_ = new NGSingleInkOverflow(adjusted_ink_overflow);
      return SetType(new_type);
    case Type::kSelf:
    case Type::kContents:
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
  return SetSingle(type, ink_overflow, size, Type::kSelf, Type::kSmallSelf);
}

NGInkOverflow::Type NGInkOverflow::SetContents(Type type,
                                               const PhysicalRect& ink_overflow,
                                               const PhysicalSize& size) {
  CheckType(type);
  if (!HasOverflow(ink_overflow, size))
    return Reset(type);
  return SetSingle(type, ink_overflow, size, Type::kContents,
                   Type::kSmallContents);
}

NGInkOverflow::Type NGInkOverflow::Set(Type type,
                                       const PhysicalRect& self,
                                       const PhysicalRect& contents,
                                       const PhysicalSize& size) {
  CheckType(type);

  if (!HasOverflow(self, size)) {
    if (!HasOverflow(contents, size))
      return Reset(type);
    return SetSingle(type, contents, size, Type::kContents,
                     Type::kSmallContents);
  }
  if (!HasOverflow(contents, size))
    return SetSingle(type, self, size, Type::kSelf, Type::kSmallSelf);

  switch (type) {
    case Type::kSelf:
    case Type::kContents:
      Reset(type);
      [[fallthrough]];
    case Type::kNotSet:
    case Type::kInvalidated:
    case Type::kNone:
    case Type::kSmallSelf:
    case Type::kSmallContents:
      container_ = new NGContainerInkOverflow(self, contents);
      return SetType(Type::kSelfAndContents);
    case Type::kSelfAndContents:
      DCHECK(container_);
      container_->ink_overflow = self;
      container_->contents_ink_overflow = contents;
      return Type::kSelfAndContents;
  }
  NOTREACHED();
}

NGInkOverflow::Type NGInkOverflow::SetTextInkOverflow(
    Type type,
    const NGInlineCursor& cursor,
    const NGTextFragmentPaintInfo& text_info,
    const ComputedStyle& style,
    const PhysicalRect& rect_in_container,
    const NGInlinePaintContext* inline_context,
    PhysicalRect* ink_overflow_out) {
  CheckType(type);
  DCHECK(type == Type::kNotSet || type == Type::kInvalidated);
  absl::optional<PhysicalRect> ink_overflow =
      ComputeTextInkOverflow(cursor, text_info, style, style.GetFont(),
                             rect_in_container, inline_context);
  if (!ink_overflow) {
    *ink_overflow_out = {PhysicalOffset(), rect_in_container.size};
    return Reset(type);
  }
  ink_overflow->ExpandEdgesToPixelBoundaries();
  *ink_overflow_out = *ink_overflow;
  return SetSelf(type, *ink_overflow, rect_in_container.size);
}

NGInkOverflow::Type NGInkOverflow::SetSvgTextInkOverflow(
    Type type,
    const NGInlineCursor& cursor,
    const NGTextFragmentPaintInfo& text_info,
    const ComputedStyle& style,
    const Font& scaled_font,
    const gfx::RectF& rect,
    float scaling_factor,
    float length_adjust_scale,
    const AffineTransform& transform,
    PhysicalRect* ink_overflow_out) {
  CheckType(type);
  DCHECK(type == Type::kNotSet || type == Type::kInvalidated);
  // Unapply length_adjust_scale because the size argument is compared with
  // Font::TextInkBounds().
  PhysicalSize item_size =
      style.IsHorizontalWritingMode()
          ? PhysicalSize(LayoutUnit(rect.width() / length_adjust_scale),
                         LayoutUnit(rect.height()))
          : PhysicalSize(LayoutUnit(rect.width()),
                         LayoutUnit(rect.height() / length_adjust_scale));
  // No |inline_context| because the decoration box is not supported for SVG.
  absl::optional<PhysicalRect> ink_overflow =
      ComputeTextInkOverflow(cursor, text_info, style, scaled_font,
                             PhysicalRect(PhysicalOffset(), item_size),
                             /* inline_context */ nullptr);
  const bool needs_transform =
      scaling_factor != 1.0f || !transform.IsIdentity();
  PhysicalSize unscaled_size = PhysicalSize::FromSizeFRound(rect.size());
  unscaled_size.Scale(1.0f / scaling_factor);
  if (!ink_overflow) {
    if (needs_transform) {
      gfx::RectF transformed_rect = transform.MapRect(rect);
      transformed_rect.Offset(-rect.x(), -rect.y());
      transformed_rect.Scale(1 / scaling_factor);
      *ink_overflow_out = PhysicalRect::EnclosingRect(transformed_rect);
      ink_overflow_out->ExpandEdgesToPixelBoundaries();
      return SetSelf(type, *ink_overflow_out, unscaled_size);
    }
    *ink_overflow_out = {PhysicalOffset(), unscaled_size};
    ink_overflow_out->ExpandEdgesToPixelBoundaries();
    return Reset(type);
  }
  // Apply length_adjust_scale before applying AffineTransform.
  if (style.IsHorizontalWritingMode()) {
    ink_overflow->SetX(LayoutUnit(ink_overflow->X() * length_adjust_scale));
    ink_overflow->SetWidth(
        LayoutUnit(ink_overflow->Width() * length_adjust_scale));
  } else {
    ink_overflow->SetY(LayoutUnit(ink_overflow->Y() * length_adjust_scale));
    ink_overflow->SetHeight(
        LayoutUnit(ink_overflow->Height() * length_adjust_scale));
  }
  if (needs_transform) {
    gfx::RectF transformed_rect(*ink_overflow);
    transformed_rect.Offset(rect.x(), rect.y());
    transformed_rect = transform.MapRect(transformed_rect);
    transformed_rect.Offset(-rect.x(), -rect.y());
    transformed_rect.Scale(1 / scaling_factor);
    *ink_overflow_out = PhysicalRect::EnclosingRect(transformed_rect);
    ink_overflow_out->ExpandEdgesToPixelBoundaries();
    return SetSelf(type, *ink_overflow_out, unscaled_size);
  }
  *ink_overflow_out = *ink_overflow;
  ink_overflow_out->ExpandEdgesToPixelBoundaries();
  return SetSelf(type, *ink_overflow, unscaled_size);
}

// static
absl::optional<PhysicalRect> NGInkOverflow::ComputeTextInkOverflow(
    const NGInlineCursor& cursor,
    const NGTextFragmentPaintInfo& text_info,
    const ComputedStyle& style,
    const Font& scaled_font,
    const PhysicalRect& rect_in_container,
    const NGInlinePaintContext* inline_context) {
  // Glyph bounds is in logical coordinate, origin at the alphabetic baseline.
  const gfx::RectF text_ink_bounds = scaled_font.TextInkBounds(text_info);
  LogicalRect ink_overflow = LogicalRect::EnclosingRect(text_ink_bounds);

  // Make the origin at the logical top of this fragment.
  if (const SimpleFontData* font_data = scaled_font.PrimaryFont()) {
    ink_overflow.offset.block_offset +=
        font_data->GetFontMetrics().FixedAscent(kAlphabeticBaseline);
  }

  if (float stroke_width = style.TextStrokeWidth()) {
    ink_overflow.Inflate(LayoutUnit::FromFloatCeil(stroke_width / 2.0f));
  }

  // Following effects, such as shadows, operate on the text decorations,
  // so compute text decoration overflow first.
  LogicalRect decoration_rect = ComputeDecorationOverflow(
      cursor, style, scaled_font, rect_in_container.offset, ink_overflow,
      inline_context);
  ink_overflow.Unite(decoration_rect);

  if (style.GetTextEmphasisMark() != TextEmphasisMark::kNone) {
    ink_overflow = ComputeEmphasisMarkOverflow(style, rect_in_container.size,
                                               ink_overflow);
  }

  const WritingMode writing_mode = style.GetWritingMode();
  if (ShadowList* text_shadow = style.TextShadow()) {
    NGLineBoxStrut text_shadow_logical_outsets =
        NGPhysicalBoxStrut::Enclosing(
            text_shadow->RectOutsetsIncludingOriginal())
            .ConvertToLineLogical({writing_mode, TextDirection::kLtr});
    ink_overflow.ExpandEdges(
        text_shadow_logical_outsets.line_over.ClampNegativeToZero(),
        text_shadow_logical_outsets.inline_end.ClampNegativeToZero(),
        text_shadow_logical_outsets.line_under.ClampNegativeToZero(),
        text_shadow_logical_outsets.inline_start.ClampNegativeToZero());
  }

  PhysicalRect local_ink_overflow =
      WritingModeConverter({writing_mode, TextDirection::kLtr},
                           rect_in_container.size)
          .ToPhysical(ink_overflow);

  // Uniting the frame rect ensures that non-ink spaces such side bearings, or
  // even space characters, are included in the visual rect for decorations.
  if (!HasOverflow(local_ink_overflow, rect_in_container.size))
    return absl::nullopt;

  local_ink_overflow.Unite({{}, rect_in_container.size});
  return local_ink_overflow;
}

// static
LogicalRect NGInkOverflow::ComputeEmphasisMarkOverflow(
    const ComputedStyle& style,
    const PhysicalSize& size,
    const LogicalRect& ink_overflow_in) {
  DCHECK(style.GetTextEmphasisMark() != TextEmphasisMark::kNone);

  LayoutUnit emphasis_mark_height = LayoutUnit(
      style.GetFont().EmphasisMarkHeight(style.TextEmphasisMarkString()));
  DCHECK_GE(emphasis_mark_height, LayoutUnit());

  LogicalRect ink_overflow = ink_overflow_in;
  if (style.GetTextEmphasisLineLogicalSide() == LineLogicalSide::kOver) {
    ink_overflow.ShiftBlockStartEdgeTo(
        std::min(ink_overflow.offset.block_offset, -emphasis_mark_height));
  } else {
    LayoutUnit logical_height =
        style.IsHorizontalWritingMode() ? size.height : size.width;
    ink_overflow.ShiftBlockEndEdgeTo(std::max(
        ink_overflow.BlockEndOffset(), logical_height + emphasis_mark_height));
  }
  return ink_overflow;
}

// static
LogicalRect NGInkOverflow::ComputeDecorationOverflow(
    const NGInlineCursor& cursor,
    const ComputedStyle& style,
    const Font& scaled_font,
    const PhysicalOffset& container_offset,
    const LogicalRect& ink_overflow,
    const NGInlinePaintContext* inline_context) {
  LogicalRect accumulated_bound;
  if (!scaled_font.PrimaryFont()) {
    return accumulated_bound;
  }
  // Text decoration from the fragment's style.
  if (style.HasAppliedTextDecorations()) {
    accumulated_bound = ComputeAppliedDecorationOverflow(
        style, scaled_font, container_offset, ink_overflow, inline_context);
  }

  bool do_highlights =
      RuntimeEnabledFeatures::HighlightOverlayPaintingEnabled();
  bool do_spelling_grammar =
      RuntimeEnabledFeatures::CSSSpellingGrammarErrorsEnabled() ||
      RuntimeEnabledFeatures::CSSPaintingForSpellingGrammarErrorsEnabled();

  if (do_highlights || do_spelling_grammar) {
    // To extract decorations due to markers, we need a fragment item and a
    // node. Ideally we would use cursor.Current().GetNode() but that's const
    // and the style functions we need to access pseudo styles take non-const
    // nodes.
    const NGFragmentItem* fragment_item = cursor.CurrentItem();
    if (!fragment_item->IsText() || fragment_item->IsGeneratedText()) {
      return accumulated_bound;
    }
    const LayoutObject* layout_object = cursor.CurrentMutableLayoutObject();
    DCHECK(layout_object);
    Text* text_node = DynamicTo<Text>(layout_object->GetNode());
    // ::first-letter passes the IsGeneratedText check but has no text node.
    if (!text_node) {
      return accumulated_bound;
    }

    DocumentMarkerController& controller = text_node->GetDocument().Markers();

    if (do_highlights) {
      DocumentMarkerVector target_markers = controller.MarkersFor(
          *text_node, DocumentMarker::MarkerTypes::TextFragment());
      if (!target_markers.empty()) {
        LogicalRect target_bound = ComputeMarkerOverflow(
            target_markers, DocumentMarker::kTextFragment, fragment_item,
            text_node, style, scaled_font, container_offset, ink_overflow,
            inline_context);
        accumulated_bound.Unite(target_bound);
      }
      DocumentMarkerVector custom_markers = controller.MarkersFor(
          *text_node, DocumentMarker::MarkerTypes::CustomHighlight());
      if (!custom_markers.empty()) {
        LogicalRect custom_bound = ComputeCustomHighlightOverflow(
            custom_markers, fragment_item, text_node, style, scaled_font,
            container_offset, ink_overflow, inline_context);
        accumulated_bound.Unite(custom_bound);
      }
    }

    if (do_spelling_grammar) {
      DocumentMarkerVector spelling_markers = controller.MarkersFor(
          *text_node, DocumentMarker::MarkerTypes::Spelling());
      LogicalRect spelling_bound = ComputeMarkerOverflow(
          spelling_markers, DocumentMarker::kSpelling, fragment_item, text_node,
          style, scaled_font, container_offset, ink_overflow, inline_context);
      accumulated_bound.Unite(spelling_bound);

      DocumentMarkerVector grammar_markers = controller.MarkersFor(
          *text_node, DocumentMarker::MarkerTypes::Grammar());
      LogicalRect grammar_bound = ComputeMarkerOverflow(
          grammar_markers, DocumentMarker::kGrammar, fragment_item, text_node,
          style, scaled_font, container_offset, ink_overflow, inline_context);
      accumulated_bound.Unite(grammar_bound);
    }
  }
  return accumulated_bound;
}

LogicalRect NGInkOverflow::ComputeAppliedDecorationOverflow(
    const ComputedStyle& style,
    const Font& scaled_font,
    const PhysicalOffset& offset_in_container,
    const LogicalRect& ink_overflow,
    const NGInlinePaintContext* inline_context,
    const AppliedTextDecoration* decoration_override) {
  DCHECK(style.HasAppliedTextDecorations() || decoration_override);
  // SVGText is currently the only reason we use decoration_override,
  // so use it as a proxy for determining minimum thickness.
  const MinimumThickness1 kMinimumThicknessIsOne(!decoration_override);
  TextDecorationInfo decoration_info(
      offset_in_container, ink_overflow.size.inline_size, style, inline_context,
      /* selection_text_decoration */ absl::nullopt, decoration_override,
      &scaled_font, kMinimumThicknessIsOne);
  NGTextDecorationOffset decoration_offset(decoration_info.TargetStyle(),
                                           style);
  gfx::RectF accumulated_bound;
  for (wtf_size_t i = 0; i < decoration_info.AppliedDecorationCount(); i++) {
    decoration_info.SetDecorationIndex(i);
    if (decoration_info.HasUnderline()) {
      decoration_info.SetUnderlineLineData(decoration_offset);
      accumulated_bound.Union(decoration_info.Bounds());
    }
    if (decoration_info.HasOverline()) {
      decoration_info.SetOverlineLineData(decoration_offset);
      accumulated_bound.Union(decoration_info.Bounds());
    }
    if (decoration_info.HasLineThrough()) {
      decoration_info.SetLineThroughLineData();
      accumulated_bound.Union(decoration_info.Bounds());
    }
    if (decoration_info.HasSpellingError() ||
        decoration_info.HasGrammarError()) {
      decoration_info.SetSpellingOrGrammarErrorLineData(decoration_offset);
      accumulated_bound.Union(decoration_info.Bounds());
    }
  }
  // Adjust the container coordinate system to the local coordinate system.
  accumulated_bound -= gfx::Vector2dF(offset_in_container);
  return LogicalRect::EnclosingRect(accumulated_bound);
}

LogicalRect NGInkOverflow::ComputeMarkerOverflow(
    const DocumentMarkerVector& markers,
    const DocumentMarker::MarkerType type,
    const NGFragmentItem* fragment_item,
    Text* text_node,
    const ComputedStyle& style,
    const Font& scaled_font,
    const PhysicalOffset& offset_in_container,
    const LogicalRect& ink_overflow,
    const NGInlinePaintContext* inline_context) {
  LogicalRect accumulated_bound;
  auto pseudo_style =
      fragment_item->Type() == NGFragmentItem::kSvgText
          ? nullptr
          : HighlightStyleUtils::HighlightPseudoStyle(
                text_node, style, NGHighlightPainter::PseudoFor(type));
  for (auto marker : markers) {
    const unsigned marker_start_offset =
        NGHighlightPainter::GetTextContentOffset(*text_node,
                                                 marker->StartOffset());
    const unsigned marker_end_offset = NGHighlightPainter::GetTextContentOffset(
        *text_node, marker->EndOffset());
    if (marker_start_offset > fragment_item->EndOffset() ||
        marker_end_offset < fragment_item->StartOffset()) {
      return LogicalRect();
    }
    LogicalRect decoration_bound;
    if (pseudo_style && pseudo_style->HasAppliedTextDecorations()) {
      decoration_bound = ComputeAppliedDecorationOverflow(
          *pseudo_style, scaled_font, offset_in_container, ink_overflow,
          inline_context);
    } else if (type == DocumentMarker::kSpelling ||
               type == DocumentMarker::kGrammar) {
      const AppliedTextDecoration synthesised{
          NGHighlightPainter::LineFor(type),
          {},
          NGHighlightPainter::ColorFor(type),
          {},
          {}};
      decoration_bound = ComputeAppliedDecorationOverflow(
          style, scaled_font, offset_in_container, ink_overflow, inline_context,
          &synthesised);
    }
    accumulated_bound.Unite(decoration_bound);
  }
  return accumulated_bound;
}

LogicalRect NGInkOverflow::ComputeCustomHighlightOverflow(
    const DocumentMarkerVector& markers,
    const NGFragmentItem* fragment_item,
    Text* text_node,
    const ComputedStyle& style,
    const Font& scaled_font,
    const PhysicalOffset& offset_in_container,
    const LogicalRect& ink_overflow,
    const NGInlinePaintContext* inline_context) {
  LogicalRect accumulated_bound;
  for (auto marker : markers) {
    const unsigned marker_start_offset =
        NGHighlightPainter::GetTextContentOffset(*text_node,
                                                 marker->StartOffset());
    const unsigned marker_end_offset = NGHighlightPainter::GetTextContentOffset(
        *text_node, marker->EndOffset());
    if (marker_start_offset > fragment_item->EndOffset() ||
        marker_end_offset < fragment_item->StartOffset()) {
      return LogicalRect();
    }

    const CustomHighlightMarker& highlight_marker =
        To<CustomHighlightMarker>(*marker);
    auto pseudo_style = fragment_item->Type() == NGFragmentItem::kSvgText
                            ? nullptr
                            : HighlightStyleUtils::HighlightPseudoStyle(
                                  text_node, style, kPseudoIdHighlight,
                                  highlight_marker.GetHighlightName());

    LogicalRect decoration_bound;
    if (pseudo_style && pseudo_style->HasAppliedTextDecorations()) {
      decoration_bound = ComputeAppliedDecorationOverflow(
          *pseudo_style, scaled_font, offset_in_container, ink_overflow,
          inline_context);
      accumulated_bound.Unite(decoration_bound);
    }
  }
  return accumulated_bound;
}

}  // namespace blink
