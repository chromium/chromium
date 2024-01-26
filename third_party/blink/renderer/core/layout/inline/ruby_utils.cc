// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/ruby_utils.h"

#include <tuple>
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/inline/inline_item_result.h"
#include "third_party/blink/renderer/core/layout/inline/line_info.h"
#include "third_party/blink/renderer/core/layout/inline/logical_line_item.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/platform/fonts/font_height.h"

namespace blink {

namespace {

std::tuple<LayoutUnit, LayoutUnit> AdjustTextOverUnderOffsetsForEmHeight(
    LayoutUnit over,
    LayoutUnit under,
    const ComputedStyle& style,
    const ShapeResultView& shape_view) {
  DCHECK_LE(over, under);
  const SimpleFontData* primary_font_data = style.GetFont().PrimaryFont();
  if (!primary_font_data)
    return std::make_pair(over, under);
  const auto font_baseline = style.GetFontBaseline();
  const LayoutUnit line_height = under - over;
  const LayoutUnit primary_ascent =
      primary_font_data->GetFontMetrics().FixedAscent(font_baseline);
  const LayoutUnit primary_descent = line_height - primary_ascent;

  DCHECK(IsMainThread());
  DEFINE_STATIC_LOCAL(Vector<ShapeResult::RunFontData>, run_fonts, ());
  DCHECK_EQ(run_fonts.size(), 0u);
  // We don't use ShapeResultView::FallbackFonts() because we can't know if the
  // primary font is actually used with FallbackFonts().
  shape_view.GetRunFontData(&run_fonts);
  const LayoutUnit kNoDiff = LayoutUnit::Max();
  LayoutUnit over_diff = kNoDiff;
  LayoutUnit under_diff = kNoDiff;
  for (const auto& run_font : run_fonts) {
    const SimpleFontData* font_data = run_font.font_data_;
    if (!font_data)
      continue;
    const FontHeight normalized_height =
        font_data->NormalizedTypoAscentAndDescent(font_baseline);
    // Floor() is better than Round().  We should not subtract pixels larger
    // than |primary_ascent - em_box.ascent|.
    const LayoutUnit current_over_diff(
        (primary_ascent - normalized_height.ascent)
            .ClampNegativeToZero()
            .Floor());
    const LayoutUnit current_under_diff(
        (primary_descent - normalized_height.descent)
            .ClampNegativeToZero()
            .Floor());
    over_diff = std::min(over_diff, current_over_diff);
    under_diff = std::min(under_diff, current_under_diff);
  }
  run_fonts.resize(0);
  if (over_diff == kNoDiff)
    over_diff = LayoutUnit();
  if (under_diff == kNoDiff)
    under_diff = LayoutUnit();
  return std::make_tuple(over + over_diff, under - under_diff);
}

}  // anonymous namespace

PhysicalRect AdjustTextRectForEmHeight(const PhysicalRect& rect,
                                       const ComputedStyle& style,
                                       const ShapeResultView* shape_view,
                                       WritingMode writing_mode) {
  if (!shape_view)
    return rect;
  const LayoutUnit line_height = IsHorizontalWritingMode(writing_mode)
                                     ? rect.size.height
                                     : rect.size.width;
  auto [over, under] = AdjustTextOverUnderOffsetsForEmHeight(
      LayoutUnit(), line_height, style, *shape_view);
  const LayoutUnit over_diff = over;
  const LayoutUnit under_diff = line_height - under;
  const LayoutUnit new_line_height = under - over;

  if (IsHorizontalWritingMode(writing_mode)) {
    return {{rect.offset.left, rect.offset.top + over_diff},
            PhysicalSize(rect.size.width, new_line_height)};
  }
  if (IsFlippedLinesWritingMode(writing_mode)) {
    return {{rect.offset.left + under_diff, rect.offset.top},
            PhysicalSize(new_line_height, rect.size.height)};
  }
  return {{rect.offset.left + over_diff, rect.offset.top},
          PhysicalSize(new_line_height, rect.size.height)};
}

AnnotationOverhang GetOverhang(const InlineItemResult& item) {
  AnnotationOverhang overhang;
  if (!item.layout_result)
    return overhang;

  const auto& column_fragment = item.layout_result->GetPhysicalFragment();

  const ComputedStyle* ruby_text_style = nullptr;
  for (const auto& child_link : column_fragment.PostLayoutChildren()) {
    const PhysicalFragment& child_fragment = *child_link.get();
    const LayoutObject* layout_object = child_fragment.GetLayoutObject();
    if (!layout_object)
      continue;
    if (layout_object->IsRubyText()) {
      ruby_text_style = layout_object->Style();
      break;
    }
  }
  if (!ruby_text_style)
    return overhang;

  // We allow overhang up to the half of ruby text font size.
  const LayoutUnit half_width_of_ruby_font =
      LayoutUnit(ruby_text_style->FontSize()) / 2;
  LayoutUnit start_overhang = half_width_of_ruby_font;
  LayoutUnit end_overhang = half_width_of_ruby_font;
  bool found_line = false;
  for (const auto& child_link : column_fragment.PostLayoutChildren()) {
    const PhysicalFragment& child_fragment = *child_link.get();
    const LayoutObject* layout_object = child_fragment.GetLayoutObject();
    if (!layout_object->IsRubyBase())
      continue;
    const ComputedStyle& base_style = child_fragment.Style();
    const auto writing_direction = base_style.GetWritingDirection();
    const LayoutUnit base_inline_size =
        LogicalFragment(writing_direction, child_fragment).InlineSize();
    // RubyBase's inline_size is always same as RubyColumn's inline_size.
    // Overhang values are offsets from RubyBase's inline edges to
    // the outmost text.
    for (const auto& base_child_link : child_fragment.PostLayoutChildren()) {
      const LayoutUnit line_inline_size =
          LogicalFragment(writing_direction, *base_child_link).InlineSize();
      if (line_inline_size == LayoutUnit())
        continue;
      found_line = true;
      const LayoutUnit start =
          base_child_link.offset
              .ConvertToLogical(writing_direction, child_fragment.Size(),
                                base_child_link.get()->Size())
              .inline_offset;
      const LayoutUnit end = base_inline_size - start - line_inline_size;
      start_overhang = std::min(start_overhang, start);
      end_overhang = std::min(end_overhang, end);
    }
  }
  if (!found_line)
    return overhang;
  overhang.start = start_overhang;
  overhang.end = end_overhang;
  return overhang;
}

bool CanApplyStartOverhang(const LineInfo& line_info,
                           LayoutUnit& start_overhang) {
  if (start_overhang <= LayoutUnit())
    return false;
  const InlineItemResults& items = line_info.Results();
  // Requires at least the current item and the previous item.
  if (items.size() < 2)
    return false;
  // Find a previous item other than kOpenTag/kCloseTag.
  // Searching items in the logical order doesn't work well with bidi
  // reordering. However, it's difficult to compute overhang after bidi
  // reordering because it affects line breaking.
  wtf_size_t previous_index = items.size() - 2;
  while ((items[previous_index].item->Type() == InlineItem::kOpenTag ||
          items[previous_index].item->Type() == InlineItem::kCloseTag) &&
         previous_index > 0) {
    --previous_index;
  }
  const InlineItemResult& previous_item = items[previous_index];
  if (previous_item.item->Type() != InlineItem::kText) {
    return false;
  }
  const InlineItem& current_item = *items.back().item;
  if (previous_item.item->Style()->FontSize() >
      current_item.Style()->FontSize())
    return false;
  start_overhang = std::min(start_overhang, previous_item.inline_size);
  return true;
}

LayoutUnit CommitPendingEndOverhang(LineInfo* line_info) {
  DCHECK(line_info);
  InlineItemResults* items = line_info->MutableResults();
  if (items->size() < 2U)
    return LayoutUnit();
  const InlineItemResult& text_item = items->back();
  if (text_item.item->Type() == InlineItem::kControl) {
    return LayoutUnit();
  }
  DCHECK(text_item.item->Type() == InlineItem::kText);
  wtf_size_t i = items->size() - 2;
  while ((*items)[i].item->Type() != InlineItem::kAtomicInline) {
    const auto type = (*items)[i].item->Type();
    if (type != InlineItem::kOpenTag && type != InlineItem::kCloseTag) {
      return LayoutUnit();
    }
    if (i-- == 0)
      return LayoutUnit();
  }
  InlineItemResult& atomic_inline_item = (*items)[i];
  if (!atomic_inline_item.layout_result->GetPhysicalFragment().IsRubyColumn()) {
    return LayoutUnit();
  }
  if (atomic_inline_item.pending_end_overhang <= LayoutUnit())
    return LayoutUnit();
  if (atomic_inline_item.item->Style()->FontSize() <
      text_item.item->Style()->FontSize())
    return LayoutUnit();
  // Ideally we should refer to inline_size of |text_item| instead of the width
  // of the InlineItem's ShapeResult. However it's impossible to compute
  // inline_size of |text_item| before calling BreakText(), and BreakText()
  // requires precise |position_| which takes |end_overhang| into account.
  LayoutUnit end_overhang =
      std::min(atomic_inline_item.pending_end_overhang,
               LayoutUnit(text_item.item->TextShapeResult()->Width()));
  DCHECK_EQ(atomic_inline_item.margins.inline_end, LayoutUnit());
  atomic_inline_item.margins.inline_end = -end_overhang;
  atomic_inline_item.inline_size -= end_overhang;
  atomic_inline_item.pending_end_overhang = LayoutUnit();
  return end_overhang;
}

AnnotationMetrics ComputeAnnotationOverflow(
    const LogicalLineItems& logical_line,
    const FontHeight& line_box_metrics,
    const ComputedStyle& line_style) {
  // Min/max position of content and annotations, ignoring line-height.
  const LayoutUnit line_over;
  LayoutUnit content_over = line_over + line_box_metrics.ascent;
  LayoutUnit content_under = content_over;

  bool has_over_annotation = false;
  bool has_under_annotation = false;

  const LayoutUnit line_under = line_over + line_box_metrics.LineHeight();
  bool has_over_emphasis = false;
  bool has_under_emphasis = false;
  for (const LogicalLineItem& item : logical_line) {
    if (!item.HasInFlowFragment())
      continue;
    if (item.IsControl())
      continue;
    LayoutUnit item_over = line_box_metrics.ascent + item.BlockOffset();
    LayoutUnit item_under = line_box_metrics.ascent + item.BlockEndOffset();
    if (item.shape_result) {
      if (const auto* style = item.Style()) {
        std::tie(item_over, item_under) = AdjustTextOverUnderOffsetsForEmHeight(
            item_over, item_under, *style, *item.shape_result);
      }
    } else {
      const auto* fragment = item.GetPhysicalFragment();
      if (fragment && fragment->IsRubyColumn()) {
        PhysicalRect rect =
            ComputeRubyEmHeightBox(*To<PhysicalBoxFragment>(fragment));
        LayoutUnit block_size;
        if (IsHorizontalWritingMode(line_style.GetWritingMode())) {
          item_under = item_over + rect.Bottom();
          item_over += rect.offset.top;
          block_size = fragment->Size().height;
        } else {
          block_size = fragment->Size().width;
          // We assume 'over' is always on right in vertical writing modes.
          // TODO(layout-dev): sideways-lr support.
          DCHECK(line_style.IsFlippedBlocksWritingMode() ||
                 line_style.IsFlippedLinesWritingMode());
          item_under = item_over + block_size;
          item_over = item_under - rect.Right();
          item_under -= rect.offset.left;
        }

        // Check if we really have an annotation.
        if (const auto& layout_result = item.layout_result) {
          LayoutUnit overflow = layout_result->AnnotationOverflow();
          if (IsFlippedLinesWritingMode(line_style.GetWritingMode()))
            overflow = -overflow;
          if (overflow < LayoutUnit())
            has_over_annotation = true;
          else if (overflow > LayoutUnit())
            has_under_annotation = true;
        }
      } else if (item.IsInlineBox()) {
        continue;
      }
    }
    content_over = std::min(content_over, item_over);
    content_under = std::max(content_under, item_under);

    if (const auto* style = item.Style()) {
      if (style->GetTextEmphasisMark() != TextEmphasisMark::kNone) {
        if (style->GetTextEmphasisLineLogicalSide() == LineLogicalSide::kOver)
          has_over_emphasis = true;
        else
          has_under_emphasis = true;
      }
    }
  }

  // Probably this is an empty line. We should secure font-size space.
  const LayoutUnit font_size(line_style.ComputedFontSize());
  if (content_under - content_over < font_size) {
    LayoutUnit half_leading = (line_box_metrics.LineHeight() - font_size) / 2;
    half_leading = half_leading.ClampNegativeToZero();
    content_over = line_over + half_leading;
    content_under = line_under - half_leading;
  }

  // Don't provide annotation space if text-emphasis exists.
  // TODO(layout-dev): If the text-emphasis is in [line_over, line_under],
  // this line can provide annotation space.
  if (has_over_emphasis)
    content_over = std::min(content_over, line_over);
  if (has_under_emphasis)
    content_under = std::max(content_under, line_under);

  // With some fonts, text fragment sizes can exceed line-height.
  // We'd like to set overflow only if we have annotations.
  // This affects fast/ruby/line-height.html on macOS.
  if (content_over < line_over && !has_over_annotation)
    content_over = line_over;
  if (content_under > line_under && !has_under_annotation)
    content_under = line_under;

  return {(line_over - content_over).ClampNegativeToZero(),
          (content_under - line_under).ClampNegativeToZero(),
          (content_over - line_over).ClampNegativeToZero(),
          (line_under - content_under).ClampNegativeToZero()};
}

namespace {

// Em height box. including contents, in the local coordinate.
PhysicalRect ComputeRubyEmHeightBox(const PhysicalFragment& fragment,
                                    const PhysicalBoxFragment& container) {
  switch (fragment.Type()) {
    case PhysicalFragment::kFragmentBox:
      return ComputeRubyEmHeightBox(To<PhysicalBoxFragment>(fragment));
    case PhysicalFragment::kFragmentLineBox:
      NOTREACHED() << "You must call LineBoxFragment::ComputeRubyEmHeightBox "
                      "explicitly.";
      break;
  }
  NOTREACHED();
  return {{}, fragment.Size()};
}

void AdjustRubyEmHeightBoxForPropagation(const PhysicalFragment& fragment,
                                         const PhysicalBoxFragment& container,
                                         PhysicalRect* overflow) {
  DCHECK(!fragment.IsLineBox());
  if (!fragment.IsCSSBox()) {
    return;
  }
  if (UNLIKELY(fragment.IsLayoutObjectDestroyedOrMoved())) {
    NOTREACHED();
    return;
  }

  const LayoutObject* layout_object = fragment.GetLayoutObject();
  DCHECK(layout_object);
  const LayoutObject* container_layout_object = container.GetLayoutObject();
  DCHECK(container_layout_object);
  if (layout_object->ShouldUseTransformFromContainer(container_layout_object)) {
    gfx::Transform transform;
    layout_object->GetTransformFromContainer(container_layout_object,
                                             PhysicalOffset(), transform);
    *overflow =
        PhysicalRect::EnclosingRect(transform.MapRect(gfx::RectF(*overflow)));
  }
}

// ComputeRubyEmHeightBox(), with transforms applied wrt container if needed.
// This does not include any offsets from the parent (including relpos).
PhysicalRect ComputeRubyEmHeightBoxForPropagation(
    const PhysicalFragment& fragment,
    const PhysicalBoxFragment& container) {
  PhysicalRect overflow = ComputeRubyEmHeightBox(fragment, container);
  AdjustRubyEmHeightBoxForPropagation(fragment, container, &overflow);
  return overflow;
}

// Chop the hanging part from scrollable overflow. Children overflow in inline
// direction should hang, which should not cause scroll.
// TODO(kojii): Should move to text fragment to make this more accurate.
void AdjustRubyEmHeightBoxForHanging(const PhysicalRect& rect,
                                     const WritingMode container_writing_mode,
                                     PhysicalRect* overflow) {
  if (IsHorizontalWritingMode(container_writing_mode)) {
    if (overflow->offset.left < rect.offset.left) {
      overflow->offset.left = rect.offset.left;
    }
    if (overflow->Right() > rect.Right()) {
      overflow->ShiftRightEdgeTo(rect.Right());
    }
  } else {
    if (overflow->offset.top < rect.offset.top) {
      overflow->offset.top = rect.offset.top;
    }
    if (overflow->Bottom() > rect.Bottom()) {
      overflow->ShiftBottomEdgeTo(rect.Bottom());
    }
  }
}

void AddRubyEmHeightBoxForInlineChild(const PhysicalFragment& child,
                                      const PhysicalBoxFragment& container,
                                      const ComputedStyle& container_style,
                                      const FragmentItem& line,
                                      bool has_hanging,
                                      const InlineCursor& cursor,
                                      PhysicalRect* overflow) {
  DCHECK(child.IsLineBox() || child.IsInlineBox());
  DCHECK(cursor.Current().Item());
  DCHECK(cursor.Current().Item()->BoxFragment() == &child ||
         cursor.Current().Item()->LineBoxFragment() == &child);
  const WritingMode container_writing_mode = container_style.GetWritingMode();
  for (InlineCursor descendants = cursor.CursorForDescendants(); descendants;) {
    const FragmentItem* item = descendants.CurrentItem();
    DCHECK(item);
    if (UNLIKELY(item->IsLayoutObjectDestroyedOrMoved())) {
      NOTREACHED();
      descendants.MoveToNextSkippingChildren();
      continue;
    }
    if (item->IsText()) {
      PhysicalRect child_scroll_overflow = item->RectInContainerFragment();
      child_scroll_overflow = AdjustTextRectForEmHeight(
          child_scroll_overflow, item->Style(), item->TextShapeResult(),
          container_writing_mode);
      if (UNLIKELY(has_hanging)) {
        AdjustRubyEmHeightBoxForHanging(line.RectInContainerFragment(),
                                        container_writing_mode,
                                        &child_scroll_overflow);
      }
      overflow->Unite(child_scroll_overflow);
      descendants.MoveToNextSkippingChildren();
      continue;
    }

    if (const PhysicalBoxFragment* child_box = item->PostLayoutBoxFragment()) {
      PhysicalRect child_scroll_overflow;
      if (child_box->GetBoxType() != PhysicalFragment::kInlineBox &&
          !child.IsRubyBox()) {
        child_scroll_overflow = item->RectInContainerFragment();
      }
      if (child_box->IsInlineBox()) {
        AddRubyEmHeightBoxForInlineChild(*child_box, container, container_style,
                                         line, has_hanging, descendants,
                                         &child_scroll_overflow);
        AdjustRubyEmHeightBoxForPropagation(*child_box, container,
                                            &child_scroll_overflow);
        if (UNLIKELY(has_hanging)) {
          AdjustRubyEmHeightBoxForHanging(line.RectInContainerFragment(),
                                          container_writing_mode,
                                          &child_scroll_overflow);
        }
      } else {
        child_scroll_overflow =
            ComputeRubyEmHeightBoxForPropagation(*child_box, container);
        child_scroll_overflow.offset += item->OffsetInContainerFragment();
      }
      overflow->Unite(child_scroll_overflow);
      descendants.MoveToNextSkippingChildren();
      continue;
    }

    // Add all children of a culled inline box; i.e., an inline box without
    // margin/border/padding etc.
    DCHECK_EQ(item->Type(), FragmentItem::kBox);
    descendants.MoveToNext();
  }
}

// Include the inline-size of the line-box in the overflow.
// Do not update block offset and block size of |overflow|.
inline void AddInlineSizeToRubyEmHeightBox(
    const PhysicalRect& rect,
    const WritingMode container_writing_mode,
    PhysicalRect* overflow) {
  PhysicalRect inline_rect;
  inline_rect.offset = rect.offset;
  if (IsHorizontalWritingMode(container_writing_mode)) {
    inline_rect.size.width = rect.size.width;
    inline_rect.offset.top = overflow->offset.top;
    inline_rect.size.height = overflow->size.height;
  } else {
    inline_rect.size.height = rect.size.height;
    inline_rect.offset.left = overflow->offset.left;
    inline_rect.size.width = overflow->size.width;
  }
  overflow->UniteEvenIfEmpty(inline_rect);
}

// Em height box. including contents, in the local coordinate.
// |ComputeRubyEmHeightBoxForLine| is not precomputed/cached because it cannot
// be computed when LineBox is generated because it needs container dimensions
// to resolve relative position of its children.
PhysicalRect ComputeRubyEmHeightBoxForLine(
    const PhysicalLineBoxFragment& line_fragment,
    const PhysicalBoxFragment& container,
    const ComputedStyle& container_style) {
  const WritingMode container_writing_mode = container_style.GetWritingMode();
  PhysicalRect overflow;
  // Make sure we include the inline-size of the line-box in the overflow.
  AddInlineSizeToRubyEmHeightBox(line_fragment.LocalRect(),
                                 container_writing_mode, &overflow);

  return overflow;
}

PhysicalRect ComputeRubyEmHeightBoxForLine(
    const PhysicalLineBoxFragment& line_fragment,
    const PhysicalBoxFragment& container,
    const ComputedStyle& container_style,
    const FragmentItem& line,
    const InlineCursor& cursor) {
  DCHECK_EQ(&line, cursor.CurrentItem());
  DCHECK_EQ(line.LineBoxFragment(), &line_fragment);

  PhysicalRect overflow;
  AddRubyEmHeightBoxForInlineChild(line_fragment, container, container_style,
                                   line, line_fragment.HasHanging(), cursor,
                                   &overflow);

  // Make sure we include the inline-size of the line-box in the overflow.
  // Note, the bottom half-leading should not be included. crbug.com/996847
  const WritingMode container_writing_mode = container_style.GetWritingMode();
  AddInlineSizeToRubyEmHeightBox(line.RectInContainerFragment(),
                                 container_writing_mode, &overflow);

  return overflow;
}

PhysicalRect ComputeRubyEmHeightBoxFromChildren(
    const PhysicalBoxFragment& fragment) {
  // TODO(kojii): See |ComputeRubyEmHeightBox|.
  const FragmentItems* items = fragment.Items();
  if (fragment.Children().empty() && !items) {
    return PhysicalRect();
  }

  // Internal struct to share logic between child fragments and child items.
  // - Inline children's overflow expands by padding end/after.
  // - Float / OOF overflow is added as is.
  // - Children not reachable by scroll overflow do not contribute to it.
  struct ComputeOverflowContext {
    STACK_ALLOCATED();

   public:
    explicit ComputeOverflowContext(const PhysicalBoxFragment& container)
        : container(container),
          style(container.Style()),
          writing_direction(style.GetWritingDirection()),
          border_inline_start(LayoutUnit(style.BorderInlineStartWidth())),
          border_block_start(LayoutUnit(style.BorderBlockStartWidth())) {
      DCHECK_EQ(&style, container.GetLayoutObject()->Style(
                            container.UsesFirstLineStyle()));

      // End and under padding are added to scroll overflow of inline children.
      // https://github.com/w3c/csswg-drafts/issues/129
      DCHECK_EQ(container.HasNonVisibleOverflow(),
                container.GetLayoutObject()->HasNonVisibleOverflow());
      if (container.HasNonVisibleOverflow()) {
        const auto* layout_object = To<LayoutBox>(container.GetLayoutObject());
        padding_strut =
            BoxStrut(LayoutUnit(), layout_object->PaddingInlineEnd(),
                     LayoutUnit(), layout_object->PaddingBlockEnd())
                .ConvertToPhysical(writing_direction);
      }
    }

    void AddChild(const PhysicalRect& child_scrollable_overflow) {
      // Do not add overflow if fragment is not reachable by scrolling.
      children_overflow.Unite(child_scrollable_overflow);
    }

    void AddFloatingOrOutOfFlowPositionedChild(
        const PhysicalFragment& child,
        const PhysicalOffset& child_offset) {
      DCHECK(child.IsFloatingOrOutOfFlowPositioned());
      PhysicalRect child_scrollable_overflow =
          ComputeRubyEmHeightBoxForPropagation(child, container);
      child_scrollable_overflow.offset += child_offset;
      AddChild(child_scrollable_overflow);
    }

    void AddLineBoxChild(const PhysicalLineBoxFragment& child,
                         const PhysicalOffset& child_offset) {
      if (padding_strut) {
        AddLineBoxRect({child_offset, child.Size()});
      }
      PhysicalRect child_scrollable_overflow =
          ComputeRubyEmHeightBoxForLine(child, container, style);
      child_scrollable_overflow.offset += child_offset;
      AddChild(child_scrollable_overflow);
    }

    void AddLineBoxChild(const FragmentItem& child,
                         const InlineCursor& cursor) {
      DCHECK_EQ(&child, cursor.CurrentItem());
      DCHECK_EQ(child.Type(), FragmentItem::kLine);
      if (padding_strut) {
        AddLineBoxRect(child.RectInContainerFragment());
      }
      const PhysicalLineBoxFragment* line_box = child.LineBoxFragment();
      DCHECK(line_box);
      PhysicalRect child_scrollable_overflow = ComputeRubyEmHeightBoxForLine(
          *line_box, container, style, child, cursor);
      AddChild(child_scrollable_overflow);
    }

    void AddLineBoxRect(const PhysicalRect& linebox_rect) {
      DCHECK(padding_strut);
      if (lineboxes_enclosing_rect) {
        lineboxes_enclosing_rect->Unite(linebox_rect);
      } else {
        lineboxes_enclosing_rect = linebox_rect;
      }
    }

    void AddPaddingToLineBoxChildren() {
      if (lineboxes_enclosing_rect) {
        DCHECK(padding_strut);
        lineboxes_enclosing_rect->Expand(*padding_strut);
        AddChild(*lineboxes_enclosing_rect);
      }
    }

    const PhysicalBoxFragment& container;
    const ComputedStyle& style;
    const WritingDirectionMode writing_direction;
    const LayoutUnit border_inline_start;
    const LayoutUnit border_block_start;
    absl::optional<PhysicalBoxStrut> padding_strut;
    absl::optional<PhysicalRect> lineboxes_enclosing_rect;
    PhysicalRect children_overflow;
  } context(fragment);

  // Traverse child items.
  if (items) {
    for (InlineCursor cursor(fragment, *items); cursor;
         cursor.MoveToNextSkippingChildren()) {
      const FragmentItem* item = cursor.CurrentItem();
      if (item->Type() == FragmentItem::kLine) {
        context.AddLineBoxChild(*item, cursor);
        continue;
      }

      if (const PhysicalBoxFragment* child_box =
              item->PostLayoutBoxFragment()) {
        if (child_box->IsFloatingOrOutOfFlowPositioned()) {
          context.AddFloatingOrOutOfFlowPositionedChild(
              *child_box, item->OffsetInContainerFragment());
        }
      }
    }
  }

  // Traverse child fragments.
  const bool add_inline_children =
      !items && fragment.IsInlineFormattingContext();
  // Only add overflow for fragments NG has not reflected into Legacy.
  // These fragments are:
  // - inline fragments,
  // - out of flow fragments whose css container is inline box.
  // TODO(layout-dev) Transforms also need to be applied to compute overflow
  // correctly. NG is not yet transform-aware. crbug.com/855965
  for (const auto& child : fragment.PostLayoutChildren()) {
    if (child->IsFloatingOrOutOfFlowPositioned()) {
      context.AddFloatingOrOutOfFlowPositionedChild(*child, child.Offset());
    } else if (add_inline_children && child->IsLineBox()) {
      context.AddLineBoxChild(To<PhysicalLineBoxFragment>(*child),
                              child.Offset());
    } else if (fragment.IsRubyColumn()) {
      PhysicalRect r = ComputeRubyEmHeightBox(*child, fragment);
      r.offset += child.offset;
      context.AddChild(r);
    }
  }

  context.AddPaddingToLineBoxChildren();

  return context.children_overflow;
}

}  // namespace

PhysicalRect ComputeRubyEmHeightBox(const PhysicalBoxFragment& box_fragment) {
  DCHECK(box_fragment.GetLayoutObject());
  // TODO(kojii): It might be that |ComputeAnnotationOverflow| should move to
  // scrollable overflow recalc, but it is to be thought out.
  if (UNLIKELY(box_fragment.IsLayoutObjectDestroyedOrMoved())) {
    NOTREACHED();
    return PhysicalRect();
  }
  const LayoutObject* layout_object = box_fragment.GetLayoutObject();
  if (box_fragment.IsRubyBox()) {
    return ComputeRubyEmHeightBoxFromChildren(box_fragment);
  }
  if (const auto* layout_box = DynamicTo<LayoutBox>(layout_object)) {
    if (box_fragment.HasNonVisibleOverflow()) {
      return PhysicalRect({}, box_fragment.Size());
    }
    // Legacy is the source of truth for overflow
    return layout_box->ScrollableOverflowRect();
  } else if (layout_object->IsLayoutInline()) {
    // Inline overflow is a union of child overflows.
    PhysicalRect overflow;
    if (box_fragment.GetBoxType() != PhysicalBoxFragment::kInlineBox) {
      overflow = PhysicalRect({}, box_fragment.Size());
    }
    for (const auto& child_fragment : box_fragment.PostLayoutChildren()) {
      PhysicalRect child_overflow =
          ComputeRubyEmHeightBoxForPropagation(*child_fragment, box_fragment);
      child_overflow.offset += child_fragment.Offset();
      overflow.Unite(child_overflow);
    }
    return overflow;
  } else {
    NOTREACHED();
  }
  return PhysicalRect({}, box_fragment.Size());
}

}  // namespace blink
