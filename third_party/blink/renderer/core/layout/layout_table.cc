/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2013 Apple Inc.
 *               All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/layout/layout_table.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_table_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_analyzer.h"
#include "third_party/blink/renderer/core/layout/layout_table_caption.h"
#include "third_party/blink/renderer/core/layout/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/layout_table_col.h"
#include "third_party/blink/renderer/core/layout/layout_table_section.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/subtree_layout_scope.h"
#include "third_party/blink/renderer/core/layout/table_layout_algorithm_auto.h"
#include "third_party/blink/renderer/core/layout/table_layout_algorithm_fixed.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/core/paint/box_painter.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/table_paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/table_painter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

LayoutTable::LayoutTable(Element* element)
    : LayoutBlock(element),
      head_(nullptr),
      foot_(nullptr),
      first_body_(nullptr),
      collapsed_borders_valid_(false),
      has_collapsed_borders_(false),
      needs_adjust_collapsed_border_joints_(false),
      needs_invalidate_collapsed_borders_for_all_cells_(false),
      collapsed_outer_borders_valid_(false),
      should_paint_all_collapsed_borders_(false),
      is_any_column_ever_collapsed_(false),
      has_col_elements_(false),
      needs_section_recalc_(false),
      column_logical_width_changed_(false),
      column_structure_changed_(false),
      column_layout_objects_valid_(false),
      no_cell_colspan_at_least_(0),
      h_spacing_(0),
      v_spacing_(0),
      collapsed_outer_border_start_(0),
      collapsed_outer_border_end_(0),
      collapsed_outer_border_before_(0),
      collapsed_outer_border_after_(0),
      collapsed_outer_border_start_overflow_(0),
      collapsed_outer_border_end_overflow_(0) {
  DCHECK(!ChildrenInline());
  effective_column_positions_.Fill(0, 1);
}

LayoutTable::~LayoutTable() = default;

void LayoutTable::StyleDidChange(StyleDifference diff,
                                 const ComputedStyle* old_style) {
  LayoutBlock::StyleDidChange(diff, old_style);

  if (ShouldCollapseBorders())
    SetHasNonCollapsedBorderDecoration(false);

  bool old_fixed_table_layout =
      old_style ? old_style->IsFixedTableLayout() : false;

  // In the collapsed border model, there is no cell spacing.
  h_spacing_ =
      ShouldCollapseBorders() ? 0 : StyleRef().HorizontalBorderSpacing();
  v_spacing_ = ShouldCollapseBorders() ? 0 : StyleRef().VerticalBorderSpacing();
  DCHECK_GE(h_spacing_, 0);
  DCHECK_GE(v_spacing_, 0);

  if (!table_layout_ ||
      StyleRef().IsFixedTableLayout() != old_fixed_table_layout) {
    if (table_layout_)
      table_layout_->WillChangeTableLayout();

    // According to the CSS2 spec, you only use fixed table layout if an
    // explicit width is specified on the table. Auto width implies auto table
    // layout.
    if (StyleRef().IsFixedTableLayout())
      table_layout_ = std::make_unique<TableLayoutAlgorithmFixed>(this);
    else
      table_layout_ = std::make_unique<TableLayoutAlgorithmAuto>(this);
  }

  if (!old_style)
    return;

  if (old_style->BorderCollapse() != StyleRef().BorderCollapse()) {
    InvalidateCollapsedBorders();
  } else {
    LayoutTableBoxComponent::InvalidateCollapsedBordersOnStyleChange(
        *this, *this, diff, *old_style);
  }

  if (LayoutTableBoxComponent::DoCellsHaveDirtyWidth(*this, *this, diff,
                                                     *old_style))
    MarkAllCellsWidthsDirtyAndOrNeedsLayout(kMarkDirtyAndNeedsLayout);
}

static inline void ResetSectionPointerIfNotBefore(LayoutTableSection*& ptr,
                                                  LayoutObject* before) {
  if (!before || !ptr)
    return;
  LayoutObject* o = before->PreviousSibling();
  while (o && o != ptr)
    o = o->PreviousSibling();
  if (!o)
    ptr = nullptr;
}

static inline bool NeedsTableSection(LayoutObject* object) {
  // Return true if 'object' can't exist in an anonymous table without being
  // wrapped in a table section box.
  EDisplay display = object->StyleRef().Display();
  return display != EDisplay::kTableCaption &&
         display != EDisplay::kTableColumnGroup &&
         display != EDisplay::kTableColumn;
}

void LayoutTable::AddChild(LayoutObject* child, LayoutObject* before_child) {
  bool wrap_in_anonymous_section = !child->IsOutOfFlowPositioned();

  if (child->IsTableCaption()) {
    wrap_in_anonymous_section = false;
  } else if (child->IsLayoutTableCol()) {
    has_col_elements_ = true;
    wrap_in_anonymous_section = false;
  } else if (child->IsTableSection()) {
    switch (child->StyleRef().Display()) {
      case EDisplay::kTableHeaderGroup:
        ResetSectionPointerIfNotBefore(head_, before_child);
        if (!head_) {
          head_ = To<LayoutTableSection>(child);
        } else {
          ResetSectionPointerIfNotBefore(first_body_, before_child);
          if (!first_body_)
            first_body_ = To<LayoutTableSection>(child);
        }
        wrap_in_anonymous_section = false;
        break;
      case EDisplay::kTableFooterGroup:
        ResetSectionPointerIfNotBefore(foot_, before_child);
        if (!foot_) {
          foot_ = To<LayoutTableSection>(child);
          wrap_in_anonymous_section = false;
          break;
        }
        FALLTHROUGH;
      case EDisplay::kTableRowGroup:
        ResetSectionPointerIfNotBefore(first_body_, before_child);
        if (!first_body_)
          first_body_ = To<LayoutTableSection>(child);
        wrap_in_anonymous_section = false;
        break;
      default:
        NOTREACHED();
    }
  } else {
    wrap_in_anonymous_section = true;
  }

  if (child->IsTableSection())
    SetNeedsSectionRecalc();

  if (!wrap_in_anonymous_section) {
    if (before_child && before_child->Parent() != this)
      before_child = SplitAnonymousBoxesAroundChild(before_child);

    LayoutBox::AddChild(child, before_child);
    return;
  }

  if (!before_child && LastChild() && LastChild()->IsTableSection() &&
      LastChild()->IsAnonymous() && !LastChild()->IsBeforeContent()) {
    LastChild()->AddChild(child);
    return;
  }

  if (before_child && !before_child->IsAnonymous() &&
      before_child->Parent() == this) {
    LayoutObject* section = before_child->PreviousSibling();
    if (section && section->IsTableSection() && section->IsAnonymous()) {
      section->AddChild(child);
      return;
    }
  }

  LayoutObject* last_box = before_child;
  while (last_box && last_box->Parent()->IsAnonymous() &&
         !last_box->IsTableSection() && NeedsTableSection(last_box))
    last_box = last_box->Parent();
  if (last_box && last_box->IsAnonymous() && last_box->IsTablePart() &&
      !IsAfterContent(last_box)) {
    if (before_child == last_box)
      before_child = last_box->SlowFirstChild();
    last_box->AddChild(child, before_child);
    return;
  }

  if (before_child && !before_child->IsTableSection() &&
      NeedsTableSection(before_child))
    before_child = nullptr;

  LayoutTableSection* section =
      LayoutTableSection::CreateAnonymousWithParent(this);
  AddChild(section, before_child);
  section->AddChild(child);
}

void LayoutTable::AddCaption(const LayoutTableCaption* caption) {
  DCHECK_EQ(captions_.Find(caption), kNotFound);
  captions_.push_back(const_cast<LayoutTableCaption*>(caption));
}

void LayoutTable::RemoveCaption(const LayoutTableCaption* old_caption) {
  wtf_size_t index = captions_.Find(old_caption);
  DCHECK_NE(index, kNotFound);
  if (index == kNotFound)
    return;

  captions_.EraseAt(index);
}

void LayoutTable::InvalidateCachedColumns() {
  column_layout_objects_valid_ = false;
  column_layout_objects_.resize(0);
}

void LayoutTable::ColumnStructureChanged() {
  column_structure_changed_ = true;
  InvalidateCachedColumns();
  // We don't really need to recompute our sections, but we do need to update
  // our column count, whether we have a column, and possibly the logical width
  // distribution too.
  SetNeedsSectionRecalc();
}

void LayoutTable::AddColumn(const LayoutTableCol*) {
  ColumnStructureChanged();
}

void LayoutTable::RemoveColumn(const LayoutTableCol*) {
  ColumnStructureChanged();
}

LayoutNGTableSectionInterface* LayoutTable::FirstBodyInterface() const {
  return FirstBody();
}

LayoutNGTableSectionInterface* LayoutTable::TopSectionInterface() const {
  return TopSection();
}

LayoutNGTableSectionInterface* LayoutTable::BottomSectionInterface() const {
  return BottomSection();
}

LayoutNGTableSectionInterface* LayoutTable::TopNonEmptySectionInterface()
    const {
  return TopNonEmptySection();
}

LayoutNGTableSectionInterface* LayoutTable::SectionBelowInterface(
    const LayoutNGTableSectionInterface* section,
    SkipEmptySectionsValue skip_empty_sections) const {
  return SectionBelow(section->ToLayoutTableSection(), skip_empty_sections);
}
LayoutNGTableSectionInterface* LayoutTable::BottomNonEmptySectionInterface()
    const {
  return BottomNonEmptySection();
}

bool LayoutTable::IsLogicalWidthAuto() const {
  const Length& style_logical_width = StyleRef().LogicalWidth();
  return (!style_logical_width.IsSpecified() ||
          !style_logical_width.IsPositive()) &&
         !style_logical_width.IsIntrinsic();
}

void LayoutTable::UpdateLogicalWidth() {
  RecalcSectionsIfNeeded();

  // Recalculate preferred logical widths now, rather than relying on them being
  // lazily recalculated, via MinPreferredLogicalWidth() further below. We might
  // not even get there.
  if (PreferredLogicalWidthsDirty())
    ComputePreferredLogicalWidths();

  if (IsFlexItemIncludingDeprecatedAndNG() || IsGridItem()) {
    // TODO(jfernandez): Investigate whether the grid layout algorithm provides
    // all the logic needed and that we're not skipping anything essential due
    // to the early return here.
    LayoutBlock::UpdateLogicalWidth();
    return;
  }

  if (IsOutOfFlowPositioned()) {
    LogicalExtentComputedValues computed_values;
    ComputePositionedLogicalWidth(computed_values);
    SetLogicalWidth(computed_values.extent_);
    SetLogicalLeft(computed_values.position_);
    SetMarginStart(computed_values.margins_.start_);
    SetMarginEnd(computed_values.margins_.end_);
  }

  LayoutBlock* cb = ContainingBlock();

  LayoutUnit available_logical_width = ContainingBlockLogicalWidthForContent();
  bool has_perpendicular_containing_block =
      cb->StyleRef().IsHorizontalWritingMode() !=
      StyleRef().IsHorizontalWritingMode();
  LayoutUnit container_width_in_inline_direction =
      has_perpendicular_containing_block
          ? PerpendicularContainingBlockLogicalHeight()
          : available_logical_width;

  if (!IsLogicalWidthAuto()) {
    SetLogicalWidth(ConvertStyleLogicalWidthToComputedWidth(
        StyleRef().LogicalWidth(), container_width_in_inline_direction));
  } else {
    // Subtract out any fixed margins from our available width for auto width
    // tables.
    LayoutUnit margin_start = MinimumValueForLength(StyleRef().MarginStart(),
                                                    available_logical_width);
    LayoutUnit margin_end =
        MinimumValueForLength(StyleRef().MarginEnd(), available_logical_width);
    LayoutUnit margin_total = margin_start + margin_end;

    LayoutUnit available_content_logical_width;
    if (HasOverrideAvailableInlineSize()) {
      available_content_logical_width =
          (OverrideAvailableInlineSize() - margin_total).ClampNegativeToZero();
    } else {
      // Subtract out our margins to get the available content width.
      available_content_logical_width =
          (container_width_in_inline_direction - margin_total)
              .ClampNegativeToZero();
      auto* containing_block_flow = DynamicTo<LayoutBlockFlow>(cb);
      if (ShrinkToAvoidFloats() && containing_block_flow &&
          containing_block_flow->ContainsFloats() &&
          !has_perpendicular_containing_block) {
        available_content_logical_width = ShrinkLogicalWidthToAvoidFloats(
            margin_start, margin_end, containing_block_flow);
      }
    }

    // Ensure we aren't bigger than our available width.
    LayoutUnit max_width = MaxPreferredLogicalWidth();
    // scaledWidthFromPercentColumns depends on m_layoutStruct in
    // TableLayoutAlgorithmAuto, which maxPreferredLogicalWidth fills in. So
    // scaledWidthFromPercentColumns has to be called after
    // maxPreferredLogicalWidth.
    LayoutUnit scaled_width = table_layout_->ScaledWidthFromPercentColumns() +
                              BordersPaddingAndSpacingInRowDirection();
    max_width = std::max(scaled_width, max_width);
    SetLogicalWidth(LayoutUnit(
        std::min(available_content_logical_width, max_width).Floor()));
  }

  // Ensure we aren't bigger than our max-width style.
  const Length& style_max_logical_width = StyleRef().LogicalMaxWidth();
  if ((style_max_logical_width.IsSpecified() &&
       !style_max_logical_width.IsNegative()) ||
      style_max_logical_width.IsIntrinsic()) {
    LayoutUnit computed_max_logical_width =
        ConvertStyleLogicalWidthToComputedWidth(style_max_logical_width,
                                                available_logical_width);
    SetLogicalWidth(LayoutUnit(
        std::min(LogicalWidth(), computed_max_logical_width).Floor()));
  }

  // Ensure we aren't smaller than our min preferred width. This MUST be done
  // after 'max-width' as we ignore it if it means we wouldn't accommodate our
  // content.
  SetLogicalWidth(
      LayoutUnit(std::max(LogicalWidth(), MinPreferredLogicalWidth()).Floor()));

  // Ensure we aren't smaller than our min-width style.
  const Length& style_min_logical_width = StyleRef().LogicalMinWidth();
  if ((style_min_logical_width.IsSpecified() &&
       !style_min_logical_width.IsNegative()) ||
      style_min_logical_width.IsIntrinsic()) {
    LayoutUnit computed_min_logical_width =
        ConvertStyleLogicalWidthToComputedWidth(style_min_logical_width,
                                                available_logical_width);
    SetLogicalWidth(LayoutUnit(
        std::max(LogicalWidth(), computed_min_logical_width).Floor()));
  }

  // Finally, with our true width determined, compute our margins for real.
  ComputedMarginValues margin_values;
  ComputeMarginsForDirection(kInlineDirection, cb, available_logical_width,
                             LogicalWidth(), margin_values.start_,
                             margin_values.end_, StyleRef().MarginStart(),
                             StyleRef().MarginEnd());
  SetMarginStart(margin_values.start_);
  SetMarginEnd(margin_values.end_);

  // We should NEVER shrink the table below the min-content logical width, or
  // else the table can't accommodate its own content which doesn't match CSS
  // nor what authors expect.
  // FIXME: When we convert to sub-pixel layout for tables we can remove the int
  // conversion. http://crbug.com/241198
  DCHECK_GE(LogicalWidth().Floor(), MinPreferredLogicalWidth().Floor());
}

// This method takes a ComputedStyle's logical width, min-width, or max-width
// length and computes its actual value.
LayoutUnit LayoutTable::ConvertStyleLogicalWidthToComputedWidth(
    const Length& style_logical_width,
    LayoutUnit available_width) const {
  if (style_logical_width.IsIntrinsic())
    return ComputeIntrinsicLogicalWidthUsing(
        style_logical_width, available_width,
        BordersPaddingAndSpacingInRowDirection());

  // HTML tables' width styles already include borders and paddings, but CSS
  // tables' width styles do not.
  LayoutUnit borders;
  bool is_css_table = !IsA<HTMLTableElement>(GetNode());
  if (is_css_table && style_logical_width.IsSpecified() &&
      style_logical_width.IsPositive() &&
      StyleRef().BoxSizing() == EBoxSizing::kContentBox) {
    borders = BorderStart() + BorderEnd() +
              (ShouldCollapseBorders() ? LayoutUnit()
                                       : PaddingStart() + PaddingEnd());
  }

  return MinimumValueForLength(style_logical_width, available_width) + borders;
}

LayoutUnit LayoutTable::ConvertStyleLogicalHeightToComputedHeight(
    const Length& style_logical_height) const {
  LayoutUnit border_and_padding_before =
      BorderBefore() +
      (ShouldCollapseBorders() ? LayoutUnit() : PaddingBefore());
  LayoutUnit border_and_padding_after =
      BorderAfter() + (ShouldCollapseBorders() ? LayoutUnit() : PaddingAfter());
  LayoutUnit border_and_padding =
      border_and_padding_before + border_and_padding_after;
  LayoutUnit computed_logical_height;
  if (style_logical_height.IsFixed()) {
    // HTML tables size as though CSS height includes border/padding, CSS tables
    // do not.
    LayoutUnit borders = LayoutUnit();
    // FIXME: We cannot apply box-sizing: content-box on <table> which other
    // browsers allow.
    if (IsA<HTMLTableElement>(GetNode()) ||
        StyleRef().BoxSizing() == EBoxSizing::kBorderBox) {
      borders = border_and_padding;
    }
    computed_logical_height =
        LayoutUnit(style_logical_height.Value() - borders);
  } else if (style_logical_height.IsPercentOrCalc()) {
    computed_logical_height =
        ComputePercentageLogicalHeight(style_logical_height);
  } else if (style_logical_height.IsIntrinsic()) {
    computed_logical_height = ComputeIntrinsicLogicalContentHeightUsing(
        style_logical_height, LogicalHeight() - border_and_padding,
        border_and_padding);
  } else {
    NOTREACHED();
  }
  return computed_logical_height.ClampNegativeToZero();
}

void LayoutTable::LayoutCaption(LayoutTableCaption& caption,
                                SubtreeLayoutScope& layouter) {
  if (!caption.NeedsLayout())
    MarkChildForPaginationRelayoutIfNeeded(caption, layouter);
  if (caption.NeedsLayout()) {
    // The margins may not be available but ensure the caption is at least
    // located beneath any previous sibling caption so that it does not
    // mistakenly think any floats in the previous caption intrude into it.
    caption.SetLogicalLocation(
        LayoutPoint(caption.MarginStart(),
                    CollapsedMarginBeforeForChild(caption) + LogicalHeight()));
    // If LayoutTableCaption ever gets a layout() function, use it here.
    caption.LayoutIfNeeded();
  }
  // Apply the margins to the location now that they are definitely available
  // from layout
  LayoutUnit caption_logical_top =
      CollapsedMarginBeforeForChild(caption) + LogicalHeight();
  caption.SetLogicalLocation(
      LayoutPoint(caption.MarginStart(), caption_logical_top));
  if (View()->GetLayoutState()->IsPaginated())
    UpdateFragmentationInfoForChild(caption);

  if (!SelfNeedsLayout())
    caption.SetShouldCheckForPaintInvalidation();

  SetLogicalHeight(LogicalHeight() + caption.LogicalHeight() +
                   CollapsedMarginBeforeForChild(caption) +
                   CollapsedMarginAfterForChild(caption));
}

void LayoutTable::LayoutSection(
    LayoutTableSection& section,
    SubtreeLayoutScope& layouter,
    LayoutUnit logical_left,
    TableHeightChangingValue table_height_changing) {
  section.SetLogicalLocation(LayoutPoint(logical_left, LogicalHeight()));
  if (column_logical_width_changed_)
    layouter.SetChildNeedsLayout(&section);
  if (!section.NeedsLayout())
    MarkChildForPaginationRelayoutIfNeeded(section, layouter);
  bool needed_layout = section.NeedsLayout();
  if (needed_layout)
    section.UpdateLayout();
  if (needed_layout || table_height_changing == kTableHeightChanging) {
    section.SetLogicalHeight(LayoutUnit(section.CalcRowLogicalHeight()));
    section.DetermineIfHeaderGroupShouldRepeat();
  }

  if (View()->GetLayoutState()->IsPaginated())
    UpdateFragmentationInfoForChild(section);
  SetLogicalHeight(LogicalHeight() + section.LogicalHeight());
}

LayoutUnit LayoutTable::LogicalHeightFromStyle() const {
  LayoutUnit computed_logical_height;
  const Length& logical_height_length = StyleRef().LogicalHeight();
  if (logical_height_length.IsIntrinsic() ||
      (logical_height_length.IsSpecified() &&
       logical_height_length.IsPositive())) {
    computed_logical_height =
        ConvertStyleLogicalHeightToComputedHeight(logical_height_length);
  }

  const Length& logical_max_height_length = StyleRef().LogicalMaxHeight();
  if (logical_max_height_length.IsFillAvailable() ||
      (logical_max_height_length.IsSpecified() &&
       !logical_max_height_length.IsNegative() &&
       !logical_max_height_length.IsMinContent() &&
       !logical_max_height_length.IsMaxContent() &&
       !logical_max_height_length.IsFitContent())) {
    LayoutUnit computed_max_logical_height =
        ConvertStyleLogicalHeightToComputedHeight(logical_max_height_length);
    computed_logical_height =
        std::min(computed_logical_height, computed_max_logical_height);
  }

  Length logical_min_height_length = StyleRef().LogicalMinHeight();
  if (logical_min_height_length.IsMinContent() ||
      logical_min_height_length.IsMaxContent() ||
      logical_min_height_length.IsFitContent())
    logical_min_height_length = Length::Auto();

  if (logical_min_height_length.IsIntrinsic() ||
      (logical_min_height_length.IsSpecified() &&
       !logical_min_height_length.IsNegative())) {
    LayoutUnit computed_min_logical_height =
        ConvertStyleLogicalHeightToComputedHeight(logical_min_height_length);
    computed_logical_height =
        std::max(computed_logical_height, computed_min_logical_height);
  }

  return computed_logical_height;
}

void LayoutTable::DistributeExtraLogicalHeight(int extra_logical_height) {
  if (extra_logical_height <= 0)
    return;

  // FIXME: Distribute the extra logical height between all table sections
  // instead of giving it all to the first one.
  if (LayoutTableSection* section = FirstBody())
    extra_logical_height -=
        section->DistributeExtraLogicalHeightToRows(extra_logical_height);

  DCHECK(!FirstBody() || !extra_logical_height);
}

void LayoutTable::SimplifiedNormalFlowLayout() {
  // FIXME: We should walk through the items in the tree in tree order to do the
  // layout here instead of walking through individual parts of the tree.
  // crbug.com/442737
  for (auto*& caption : captions_)
    caption->LayoutIfNeeded();

  for (LayoutTableSection* section = TopSection(); section;
       section = SectionBelow(section)) {
    section->LayoutIfNeeded();
    section->LayoutRows();
    section->ComputeLayoutOverflowFromDescendants();
    section->UpdateAfterLayout();
  }
}

bool LayoutTable::RecalcLayoutOverflow() {
  RecalcSelfLayoutOverflow();

  if (!ChildNeedsLayoutOverflowRecalc())
    return false;
  ClearChildNeedsLayoutOverflowRecalc();

  // If the table sections we keep pointers to have gone away then the table
  // will be rebuilt and overflow will get recalculated anyway so return early.
  if (NeedsSectionRecalc())
    return false;

  bool children_layout_overflow_changed = false;
  for (LayoutTableSection* section = TopSection(); section;
       section = SectionBelow(section)) {
    children_layout_overflow_changed =
        section->RecalcLayoutOverflow() || children_layout_overflow_changed;
  }
  return RecalcPositionedDescendantsLayoutOverflow() ||
         children_layout_overflow_changed;
}

void LayoutTable::RecalcVisualOverflow() {
  for (auto* caption : captions_) {
    if (!caption->HasSelfPaintingLayer())
      caption->RecalcVisualOverflow();
  }

  for (LayoutTableSection* section = TopSection(); section;
       section = SectionBelow(section)) {
    if (!section->HasSelfPaintingLayer())
      section->RecalcVisualOverflow();
  }

  RecalcPositionedDescendantsVisualOverflow();
  RecalcSelfVisualOverflow();
}

void LayoutTable::UpdateLayout() {
  DCHECK(NeedsLayout());
  LayoutAnalyzer::Scope analyzer(*this);

  if (SimplifiedLayout())
    return;

  // Note: LayoutTable is handled differently than other LayoutBlocks and the
  // LayoutScope
  //       must be created before the table begins laying out.
  TextAutosizer::LayoutScope text_autosizer_layout_scope(this);

  RecalcSectionsIfNeeded();

  SubtreeLayoutScope layouter(*this);

  {
    LayoutState state(*this);
    LayoutUnit old_logical_width = LogicalWidth();
    LayoutUnit old_logical_height = LogicalHeight();

    SetLogicalHeight(LayoutUnit());
    UpdateLogicalWidth();

    if (LogicalWidth() != old_logical_width) {
      for (unsigned i = 0; i < captions_.size(); i++) {
        layouter.SetNeedsLayout(captions_[i],
                                layout_invalidation_reason::kTableChanged);
      }
    }
    // FIXME: The optimisation below doesn't work since the internal table
    // layout could have changed. We need to add a flag to the table
    // layout that tells us if something has changed in the min max
    // calculations to do it correctly.
    // if ( oldWidth != width() || columns.size() + 1 != columnPos.size() )
    table_layout_->UpdateLayout();

    // Lay out top captions.
    // FIXME: Collapse caption margin.
    for (unsigned i = 0; i < captions_.size(); i++) {
      if (captions_[i]->StyleRef().CaptionSide() == ECaptionSide::kBottom)
        continue;
      LayoutCaption(*captions_[i], layouter);
    }

    LayoutTableSection* top_section = TopSection();
    LayoutTableSection* bottom_section = BottomSection();

    // This is the border-before edge of the "table box", relative to the "table
    // wrapper box", i.e. right after all top captions.
    // https://www.w3.org/TR/2011/REC-CSS2-20110607/tables.html#model
    LayoutUnit table_box_logical_top = LogicalHeight();

    bool collapsing = ShouldCollapseBorders();
    LayoutUnit border_and_padding_before =
        BorderBefore() + (collapsing ? LayoutUnit() : PaddingBefore());
    LayoutUnit border_and_padding_after =
        BorderAfter() + (collapsing ? LayoutUnit() : PaddingAfter());

    SetLogicalHeight(table_box_logical_top + border_and_padding_before);

    LayoutUnit section_logical_left = LayoutUnit(
        StyleRef().IsLeftToRightDirection() ? BorderStart() : BorderEnd());
    if (!collapsing) {
      section_logical_left +=
          StyleRef().IsLeftToRightDirection() ? PaddingStart() : PaddingEnd();
    }
    LayoutUnit current_available_logical_height =
        AvailableLogicalHeight(kIncludeMarginBorderPadding);
    TableHeightChangingValue table_height_changing =
        old_available_logical_height_ && old_available_logical_height_ !=
                                             current_available_logical_height
            ? kTableHeightChanging
            : kTableHeightNotChanging;
    old_available_logical_height_ = current_available_logical_height;

    // Lay out table footer to get its raw height. This will help us decide
    // if we can repeat it in each page/column.
    LayoutTableSection* footer = Footer();
    if (footer) {
      if (footer->GetPaginationBreakability() != kAllowAnyBreaks) {
        footer->LayoutIfNeeded();
        int footer_logical_height = footer->CalcRowLogicalHeight();
        footer->SetLogicalHeight(LayoutUnit(footer_logical_height));
      }
      footer->DetermineIfFooterGroupShouldRepeat();
    }

    // Lay out table header group.
    LayoutTableSection* header = Header();
    if (header) {
      LayoutSection(*header, layouter, section_logical_left,
                    table_height_changing);
    }

    LayoutUnit original_offset_for_table_headers =
        state.HeightOffsetForTableHeaders();
    LayoutUnit offset_for_table_headers = original_offset_for_table_headers;
    LayoutUnit original_offset_for_table_footers =
        state.HeightOffsetForTableFooters();
    LayoutUnit offset_for_table_footers = original_offset_for_table_footers;
    if (state.IsPaginated() && IsPageLogicalHeightKnown()) {
      // If the repeating header group allows at least one row of content,
      // then store the offset for other sections to offset their rows
      // against.
      if (header && header->IsRepeatingHeaderGroup()) {
        offset_for_table_headers += header->LogicalHeight();
        // Don't include any strut in the header group - we only want the
        // height from its content.
        if (LayoutTableRow* row = header->FirstRow())
          offset_for_table_headers -= row->PaginationStrut();
        SetRowOffsetFromRepeatingHeader(offset_for_table_headers);
      }

      if (footer && footer->IsRepeatingFooterGroup()) {
        offset_for_table_footers += footer->LogicalHeight();
        SetRowOffsetFromRepeatingFooter(offset_for_table_footers);
      }
    }
    state.SetHeightOffsetForTableHeaders(offset_for_table_headers);
    state.SetHeightOffsetForTableFooters(offset_for_table_footers);

    // Lay out table body groups, and column groups.
    for (LayoutObject* child = FirstChild(); child;
         child = child->NextSibling()) {
      if (child->IsTableSection()) {
        if (child != Header() && child != Footer()) {
          LayoutTableSection& section = *To<LayoutTableSection>(child);
          LayoutSection(section, layouter, section_logical_left,
                        table_height_changing);
        }
      } else if (child->IsLayoutTableCol()) {
        child->LayoutIfNeeded();
      } else {
        DCHECK(child->IsTableCaption());
      }
    }
    // Reset these so they don't affect the layout of footers or captions.
    state.SetHeightOffsetForTableHeaders(original_offset_for_table_headers);
    state.SetHeightOffsetForTableFooters(original_offset_for_table_footers);

    // Change logical width according to any collapsed columns.
    Vector<int> col_collapsed_width;
    AdjustWidthsForCollapsedColumns(col_collapsed_width);

    // Lay out table footer.
    if (LayoutTableSection* section = Footer()) {
      LayoutSection(*section, layouter, section_logical_left,
                    table_height_changing);
    }

    SetLogicalHeight(table_box_logical_top + border_and_padding_before);

    LayoutUnit computed_logical_height = LogicalHeightFromStyle();
    LayoutUnit total_section_logical_height;
    if (top_section) {
      total_section_logical_height =
          bottom_section->LogicalBottom() - top_section->LogicalTop();
    }

    if (!state.IsPaginated() ||
        !CrossesPageBoundary(table_box_logical_top, computed_logical_height)) {
      DistributeExtraLogicalHeight(
          FloorToInt(computed_logical_height - total_section_logical_height));
    }

    LayoutUnit logical_offset =
        top_section ? top_section->LogicalTop() : LayoutUnit();
    for (LayoutTableSection* section = top_section; section;
         section = SectionBelow(section)) {
      section->SetLogicalTop(logical_offset);
      section->LayoutRows();
      if (!IsAnyColumnEverCollapsed()) {
        if (col_collapsed_width.size())
          SetIsAnyColumnEverCollapsed();
      }
      if (IsAnyColumnEverCollapsed())
        section->UpdateLogicalWidthForCollapsedCells(col_collapsed_width);
      logical_offset += section->LogicalHeight();
    }

    if (!top_section &&
        computed_logical_height > total_section_logical_height &&
        !GetDocument().InQuirksMode()) {
      // Completely empty tables (with no sections or anything) should at least
      // honor specified height in strict mode.
      SetLogicalHeight(LogicalHeight() + computed_logical_height);
    }

    // position the table sections
    LayoutTableSection* section = top_section;
    while (section) {
      section->SetLogicalLocation(
          LayoutPoint(section_logical_left, LogicalHeight()));

      SetLogicalHeight(LogicalHeight() + section->LogicalHeight());

      section->UpdateAfterLayout();

      section = SectionBelow(section);
    }

    SetLogicalHeight(LogicalHeight() + border_and_padding_after);

    // Lay out bottom captions.
    for (unsigned i = 0; i < captions_.size(); i++) {
      if (captions_[i]->StyleRef().CaptionSide() != ECaptionSide::kBottom)
        continue;
      LayoutCaption(*captions_[i], layouter);
    }

    UpdateLogicalHeight();

    // table can be containing block of positioned elements.
    bool dimension_changed = old_logical_width != LogicalWidth() ||
                             old_logical_height != LogicalHeight();
    LayoutPositionedObjects(dimension_changed);

    ComputeLayoutOverflow(ClientLogicalBottom());
    UpdateAfterLayout();
  }

  // FIXME: This value isn't the intrinsic content logical height, but we need
  // to update the value as its used by flexbox layout. crbug.com/367324
  SetIntrinsicContentLogicalHeight(ContentLogicalHeight());

  column_logical_width_changed_ = false;
  ClearNeedsLayout();
}

void LayoutTable::AdjustWidthsForCollapsedColumns(
    Vector<int>& col_collapsed_width) {
  DCHECK(!col_collapsed_width.size());
  if (!RuntimeEnabledFeatures::VisibilityCollapseColumnEnabled())
    return;

  unsigned n_eff_cols = NumEffectiveColumns();

  // Update vector of collapsed widths.
  for (unsigned i = 0; i < n_eff_cols; ++i) {
    // TODO(joysyu): Here, we are at O(n^2) for every table that has ever had a
    // collapsed column. ColElementAtAbsoluteColumn() is currently O(n);
    // ideally, it would be O(1). We have to improve the runtime before shipping
    // visibility:collapse for columns. See discussion at
    // https://chromium-review.googlesource.com/c/chromium/src/+/602506/18/third_party/WebKit/Source/core/layout/LayoutTable.cpp
    if (IsAbsoluteColumnCollapsed(EffectiveColumnToAbsoluteColumn(i))) {
      if (!col_collapsed_width.size())
        col_collapsed_width.Grow(n_eff_cols);
      col_collapsed_width[i] =
          EffectiveColumnPositions()[i + 1] - EffectiveColumnPositions()[i];
    }
  }

  if (!col_collapsed_width.size())
    return;

  // Adjust column positions according to collapsed widths.
  int total_collapsed_width = 0;
  for (unsigned i = 0; i < n_eff_cols; ++i) {
    total_collapsed_width += col_collapsed_width[i];
    SetEffectiveColumnPosition(
        i + 1, EffectiveColumnPositions()[i + 1] - total_collapsed_width);
  }

  SetLogicalWidth(LogicalWidth() - total_collapsed_width);
  DCHECK_GE(LogicalWidth(), 0);
}

bool LayoutTable::IsAbsoluteColumnCollapsed(
    unsigned absolute_column_index) const {
  ColAndColGroup colElement = ColElementAtAbsoluteColumn(absolute_column_index);
  LayoutTableCol* col = colElement.col;
  LayoutTableCol* colgroup = colElement.colgroup;
  return (col && col->StyleRef().Visibility() == EVisibility::kCollapse) ||
         (colgroup &&
          colgroup->StyleRef().Visibility() == EVisibility::kCollapse);
}

void LayoutTable::InvalidateCollapsedBorders() {
  collapsed_borders_valid_ = false;
  needs_invalidate_collapsed_borders_for_all_cells_ = true;
  collapsed_outer_borders_valid_ = false;
  SetShouldCheckForPaintInvalidation();
}

void LayoutTable::InvalidateCollapsedBordersForAllCellsIfNeeded() {
  DCHECK(ShouldCollapseBorders());

  if (!needs_invalidate_collapsed_borders_for_all_cells_)
    return;
  needs_invalidate_collapsed_borders_for_all_cells_ = false;

  for (LayoutObject* section = FirstChild(); section;
       section = section->NextSibling()) {
    if (!section->IsTableSection())
      continue;
    for (LayoutTableRow* row = To<LayoutTableSection>(section)->FirstRow(); row;
         row = row->NextRow()) {
      for (LayoutTableCell* cell = row->FirstCell(); cell;
           cell = cell->NextCell()) {
        DCHECK_EQ(cell->Table(), this);
        cell->InvalidateCollapsedBorderValues();
        cell->SetHasNonCollapsedBorderDecoration(
            !ShouldCollapseBorders() && cell->StyleRef().HasBorderDecoration());
      }
    }
  }
}

void LayoutTable::ComputeVisualOverflow(bool) {
  LayoutRect previous_visual_overflow_rect = VisualOverflowRect();
  ClearVisualOverflow();
  AddVisualOverflowFromChildren();

  AddVisualEffectOverflow();
  AddVisualOverflowFromTheme();

  if (VisualOverflowRect() != previous_visual_overflow_rect) {
    SetShouldCheckForPaintInvalidation();
    GetFrameView()->SetIntersectionObservationState(LocalFrameView::kDesired);
  }
}

void LayoutTable::AddVisualOverflowFromChildren() {
  // Add overflow from borders.
  // Technically it's odd that we are incorporating the borders into layout
  // overflow, which is only supposed to be about overflow from our
  // descendant objects, but since tables don't support overflow:auto, this
  // works out fine.
  UpdateCollapsedOuterBorders();
  if (ShouldCollapseBorders() && (collapsed_outer_border_start_overflow_ ||
                                  collapsed_outer_border_end_overflow_)) {
    LogicalToPhysical<LayoutUnit> physical_border_overflow(
        StyleRef().GetWritingMode(), StyleRef().Direction(),
        LayoutUnit(collapsed_outer_border_start_overflow_),
        LayoutUnit(collapsed_outer_border_end_overflow_), LayoutUnit(),
        LayoutUnit());
    LayoutRect border_overflow(PixelSnappedBorderBoxRect());
    border_overflow.Expand(LayoutRectOutsets(
        physical_border_overflow.Top(), physical_border_overflow.Right(),
        physical_border_overflow.Bottom(), physical_border_overflow.Left()));
    AddSelfVisualOverflow(border_overflow);
  }

  // Add overflow from our caption.
  for (auto* caption : captions_)
    AddVisualOverflowFromChild(*caption);

  // Add overflow from our sections.
  for (LayoutTableSection* section = TopSection(); section;
       section = SectionBelow(section))
    AddVisualOverflowFromChild(*section);
}

void LayoutTable::AddLayoutOverflowFromChildren() {
  // Add overflow from borders.
  // Technically it's odd that we are incorporating the borders into layout
  // overflow, which is only supposed to be about overflow from our
  // descendant objects, but since tables don't support overflow:auto, this
  // works out fine.
  UpdateCollapsedOuterBorders();
  if (ShouldCollapseBorders() && (collapsed_outer_border_start_overflow_ ||
                                  collapsed_outer_border_end_overflow_)) {
    LogicalToPhysical<LayoutUnit> physical_border_overflow(
        StyleRef().GetWritingMode(), StyleRef().Direction(),
        LayoutUnit(collapsed_outer_border_start_overflow_),
        LayoutUnit(collapsed_outer_border_end_overflow_), LayoutUnit(),
        LayoutUnit());
    LayoutRect border_overflow(PixelSnappedBorderBoxRect());
    border_overflow.Expand(LayoutRectOutsets(
        physical_border_overflow.Top(), physical_border_overflow.Right(),
        physical_border_overflow.Bottom(), physical_border_overflow.Left()));
    AddLayoutOverflow(border_overflow);
  }

  // Add overflow from our caption.
  for (unsigned i = 0; i < captions_.size(); i++)
    AddLayoutOverflowFromChild(*captions_[i]);

  // Add overflow from our sections.
  for (LayoutTableSection* section = TopSection(); section;
       section = SectionBelow(section))
    AddLayoutOverflowFromChild(*section);
}

void LayoutTable::PaintObject(const PaintInfo& paint_info,
                              const PhysicalOffset& paint_offset) const {
  TablePainter(*this).PaintObject(paint_info, paint_offset);
}

void LayoutTable::SubtractCaptionRect(PhysicalRect& rect) const {
  for (unsigned i = 0; i < captions_.size(); i++) {
    LayoutUnit caption_logical_height = captions_[i]->LogicalHeight() +
                                        captions_[i]->MarginBefore() +
                                        captions_[i]->MarginAfter();
    bool caption_is_before =
        (captions_[i]->StyleRef().CaptionSide() != ECaptionSide::kBottom) ^
        StyleRef().IsFlippedBlocksWritingMode();
    if (StyleRef().IsHorizontalWritingMode()) {
      rect.size.height -= caption_logical_height;
      if (caption_is_before)
        rect.offset.top += caption_logical_height;
    } else {
      rect.size.width -= caption_logical_height;
      if (caption_is_before)
        rect.offset.left += caption_logical_height;
    }
  }
}

void LayoutTable::MarkAllCellsWidthsDirtyAndOrNeedsLayout(
    WhatToMarkAllCells what_to_mark) {
  for (LayoutObject* child = Children()->FirstChild(); child;
       child = child->NextSibling()) {
    if (!child->IsTableSection())
      continue;
    LayoutTableSection* section = To<LayoutTableSection>(child);
    section->MarkAllCellsWidthsDirtyAndOrNeedsLayout(what_to_mark);
  }
}

void LayoutTable::PaintBoxDecorationBackground(
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) const {
  TablePainter(*this).PaintBoxDecorationBackground(paint_info, paint_offset);
}

void LayoutTable::PaintMask(const PaintInfo& paint_info,
                            const PhysicalOffset& paint_offset) const {
  TablePainter(*this).PaintMask(paint_info, paint_offset);
}

void LayoutTable::ComputeIntrinsicLogicalWidths(LayoutUnit& min_width,
                                                LayoutUnit& max_width) const {
  RecalcSectionsIfNeeded();
  // FIXME: Restructure the table layout code so that we can make this method
  // const.
  const_cast<LayoutTable*>(this)->table_layout_->ComputeIntrinsicLogicalWidths(
      min_width, max_width);

  // FIXME: We should include captions widths here like we do in
  // computePreferredLogicalWidths.
}

void LayoutTable::ComputePreferredLogicalWidths() {
  DCHECK(PreferredLogicalWidthsDirty());

  ComputeIntrinsicLogicalWidths(min_preferred_logical_width_,
                                max_preferred_logical_width_);

  int borders_padding_and_spacing =
      BordersPaddingAndSpacingInRowDirection().ToInt();
  min_preferred_logical_width_ += borders_padding_and_spacing;
  max_preferred_logical_width_ += borders_padding_and_spacing;

  table_layout_->ApplyPreferredLogicalWidthQuirks(min_preferred_logical_width_,
                                                  max_preferred_logical_width_);

  for (unsigned i = 0; i < captions_.size(); i++) {
    min_preferred_logical_width_ = std::max(
        min_preferred_logical_width_, captions_[i]->MinPreferredLogicalWidth());
    // Note: using captions' min-width is intentional here:
    max_preferred_logical_width_ = std::max(
        max_preferred_logical_width_, captions_[i]->MinPreferredLogicalWidth());
  }

  const ComputedStyle& style_to_use = StyleRef();
  // FIXME: This should probably be checking for isSpecified since you should be
  // able to use percentage or calc values for min-width.
  if (style_to_use.LogicalMinWidth().IsFixed() &&
      style_to_use.LogicalMinWidth().Value() > 0) {
    max_preferred_logical_width_ =
        std::max(max_preferred_logical_width_,
                 AdjustContentBoxLogicalWidthForBoxSizing(
                     style_to_use.LogicalMinWidth().Value()));
    min_preferred_logical_width_ =
        std::max(min_preferred_logical_width_,
                 AdjustContentBoxLogicalWidthForBoxSizing(
                     style_to_use.LogicalMinWidth().Value()));
  }

  // FIXME: This should probably be checking for isSpecified since you should be
  // able to use percentage or calc values for maxWidth.
  if (style_to_use.LogicalMaxWidth().IsFixed()) {
    // We don't constrain m_minPreferredLogicalWidth as the table should be at
    // least the size of its min-content, regardless of 'max-width'.
    max_preferred_logical_width_ =
        std::min(max_preferred_logical_width_,
                 AdjustContentBoxLogicalWidthForBoxSizing(
                     style_to_use.LogicalMaxWidth().Value()));
  }

  // 2 cases need this:
  // 1. When max_preferred_logical_width is shrunk to the specified max-width in
  //    the block above but max-width < min_preferred_logical_width.
  // 2. We buggily calculate min > max for some tables with colspans and
  //    percent widths. See fast/table/spans-min-greater-than-max-crash.html and
  //    http://crbug.com/857185
  max_preferred_logical_width_ =
      std::max(min_preferred_logical_width_, max_preferred_logical_width_);

  // FIXME: We should be adding borderAndPaddingLogicalWidth here, but
  // m_tableLayout->computePreferredLogicalWidths already does, so a bunch of
  // tests break doing this naively.
  ClearPreferredLogicalWidthsDirty();
}

LayoutTableSection* LayoutTable::TopNonEmptySection() const {
  LayoutTableSection* section = TopSection();
  if (section && !section->NumRows())
    section = SectionBelow(section, kSkipEmptySections);
  return section;
}

LayoutTableSection* LayoutTable::BottomNonEmptySection() const {
  LayoutTableSection* section = BottomSection();
  if (section && !section->NumRows())
    section = SectionAbove(section, kSkipEmptySections);
  return section;
}

void LayoutTable::SplitEffectiveColumn(unsigned index, unsigned first_span) {
  // We split the column at |index|, taking |firstSpan| cells from the span.
  DCHECK_GT(effective_columns_[index].span, first_span);
  effective_columns_.insert(index, first_span);
  effective_columns_[index + 1].span -= first_span;

  // Propagate the change in our columns representation to the sections that
  // don't need cell recalc. If they do, they will be synced up directly with
  // m_columns later.
  for (LayoutObject* child = FirstChild(); child;
       child = child->NextSibling()) {
    if (!child->IsTableSection())
      continue;

    LayoutTableSection* section = To<LayoutTableSection>(child);
    if (section->NeedsCellRecalc())
      continue;

    section->SplitEffectiveColumn(index, first_span);
  }

  effective_column_positions_.Grow(NumEffectiveColumns() + 1);
}

void LayoutTable::AppendEffectiveColumn(unsigned span) {
  unsigned new_column_index = effective_columns_.size();
  effective_columns_.push_back(span);

  // Unless the table has cell(s) with colspan that exceed the number of columns
  // afforded by the other rows in the table we can use the fast path when
  // mapping columns to effective columns.
  if (span == 1 && no_cell_colspan_at_least_ + 1 == NumEffectiveColumns()) {
    no_cell_colspan_at_least_++;
  }

  // Propagate the change in our columns representation to the sections that
  // don't need cell recalc. If they do, they will be synced up directly with
  // m_columns later.
  for (LayoutObject* child = FirstChild(); child;
       child = child->NextSibling()) {
    if (!child->IsTableSection())
      continue;

    LayoutTableSection* section = To<LayoutTableSection>(child);
    if (section->NeedsCellRecalc())
      continue;

    section->AppendEffectiveColumn(new_column_index);
  }

  effective_column_positions_.Grow(NumEffectiveColumns() + 1);
}

LayoutTableCol* LayoutTable::FirstColumn() const {
  for (LayoutObject* child = FirstChild(); child;
       child = child->NextSibling()) {
    if (child->IsLayoutTableCol())
      return ToLayoutTableCol(child);
  }

  return nullptr;
}

void LayoutTable::UpdateColumnCache() const {
  DCHECK(has_col_elements_);
  DCHECK(column_layout_objects_.IsEmpty());
  DCHECK(!column_layout_objects_valid_);

  for (LayoutTableCol* column_layout_object = FirstColumn();
       column_layout_object;
       column_layout_object = column_layout_object->NextColumn()) {
    if (column_layout_object->IsTableColumnGroupWithColumnChildren())
      continue;
    column_layout_objects_.push_back(column_layout_object);
  }
  column_layout_objects_valid_ = true;
  // TODO(joysyu): There may be an optimization opportunity to set
  // is_any_column_ever_collapsed_ to false here.
}

LayoutTable::ColAndColGroup LayoutTable::SlowColElementAtAbsoluteColumn(
    unsigned absolute_column_index) const {
  DCHECK(has_col_elements_);

  if (!column_layout_objects_valid_)
    UpdateColumnCache();

  unsigned column_count = 0;
  for (unsigned i = 0; i < column_layout_objects_.size(); i++) {
    LayoutTableCol* column_layout_object = column_layout_objects_[i];
    DCHECK(!column_layout_object->IsTableColumnGroupWithColumnChildren());
    unsigned span = column_layout_object->Span();
    unsigned start_col = column_count;
    DCHECK_GE(span, 1u);
    unsigned end_col = column_count + span - 1;
    column_count += span;
    if (column_count > absolute_column_index) {
      ColAndColGroup col_and_col_group;
      bool is_at_start_edge = start_col == absolute_column_index;
      bool is_at_end_edge = end_col == absolute_column_index;
      if (column_layout_object->IsTableColumnGroup()) {
        col_and_col_group.colgroup = column_layout_object;
        col_and_col_group.adjoins_start_border_of_col_group = is_at_start_edge;
        col_and_col_group.adjoins_end_border_of_col_group = is_at_end_edge;
      } else {
        col_and_col_group.col = column_layout_object;
        col_and_col_group.colgroup =
            column_layout_object->EnclosingColumnGroup();
        if (col_and_col_group.colgroup) {
          col_and_col_group.adjoins_start_border_of_col_group =
              is_at_start_edge && !col_and_col_group.col->PreviousSibling();
          col_and_col_group.adjoins_end_border_of_col_group =
              is_at_end_edge && !col_and_col_group.col->NextSibling();
        }
      }
      return col_and_col_group;
    }
  }
  return ColAndColGroup();
}

void LayoutTable::RecalcSections() const {
  DCHECK(needs_section_recalc_);

  head_ = nullptr;
  foot_ = nullptr;
  first_body_ = nullptr;
  has_col_elements_ = false;

  // We need to get valid pointers to caption, head, foot and first body again
  LayoutObject* next_sibling;
  for (LayoutObject* child = FirstChild(); child; child = next_sibling) {
    next_sibling = child->NextSibling();
    switch (child->StyleRef().Display()) {
      case EDisplay::kTableColumn:
      case EDisplay::kTableColumnGroup:
        has_col_elements_ = true;
        break;
      case EDisplay::kTableHeaderGroup:
        if (child->IsTableSection()) {
          LayoutTableSection* section = To<LayoutTableSection>(child);
          if (!head_)
            head_ = section;
          else if (!first_body_)
            first_body_ = section;
          section->RecalcCellsIfNeeded();
        }
        break;
      case EDisplay::kTableFooterGroup:
        if (child->IsTableSection()) {
          LayoutTableSection* section = To<LayoutTableSection>(child);
          if (!foot_)
            foot_ = section;
          else if (!first_body_)
            first_body_ = section;
          section->RecalcCellsIfNeeded();
        }
        break;
      case EDisplay::kTableRowGroup:
        if (child->IsTableSection()) {
          LayoutTableSection* section = To<LayoutTableSection>(child);
          if (!first_body_)
            first_body_ = section;
          section->RecalcCellsIfNeeded();
        }
        break;
      default:
        break;
    }
  }

  // repair column count (addChild can grow it too much, because it always adds
  // elements to the last row of a section)
  unsigned max_cols = 0;
  for (LayoutObject* child = FirstChild(); child;
       child = child->NextSibling()) {
    if (child->IsTableSection()) {
      LayoutTableSection* section = To<LayoutTableSection>(child);
      if (column_structure_changed_) {
        section->MarkAllCellsWidthsDirtyAndOrNeedsLayout(
            LayoutTable::kMarkDirtyAndNeedsLayout);
      }
      unsigned section_cols = section->NumEffectiveColumns();
      if (section_cols > max_cols)
        max_cols = section_cols;
    }
  }
  column_structure_changed_ = false;

  effective_columns_.resize(max_cols);
  effective_column_positions_.resize(max_cols + 1);
  no_cell_colspan_at_least_ = CalcNoCellColspanAtLeast();

  DCHECK(SelfNeedsLayout());

  needs_section_recalc_ = false;
}

LayoutUnit LayoutTable::BorderLeft() const {
  if (ShouldCollapseBorders()) {
    UpdateCollapsedOuterBorders();
    return LayoutUnit(LogicalCollapsedOuterBorderToPhysical().Left());
  }
  return LayoutUnit(LayoutBlock::BorderLeft().ToInt());
}

LayoutUnit LayoutTable::BorderRight() const {
  if (ShouldCollapseBorders()) {
    UpdateCollapsedOuterBorders();
    return LayoutUnit(LogicalCollapsedOuterBorderToPhysical().Right());
  }
  return LayoutUnit(LayoutBlock::BorderRight().ToInt());
}

LayoutUnit LayoutTable::BorderTop() const {
  if (ShouldCollapseBorders()) {
    UpdateCollapsedOuterBorders();
    return LayoutUnit(LogicalCollapsedOuterBorderToPhysical().Top());
  }
  return LayoutUnit(LayoutBlock::BorderTop().ToInt());
}

LayoutUnit LayoutTable::BorderBottom() const {
  if (ShouldCollapseBorders()) {
    UpdateCollapsedOuterBorders();
    return LayoutUnit(LogicalCollapsedOuterBorderToPhysical().Bottom());
  }
  return LayoutUnit(LayoutBlock::BorderBottom().ToInt());
}

LayoutTableSection* LayoutTable::SectionAbove(
    const LayoutTableSection* section,
    SkipEmptySectionsValue skip_empty_sections) const {
  RecalcSectionsIfNeeded();

  if (section == head_)
    return nullptr;

  LayoutObject* prev_section =
      section == foot_ ? LastChild() : section->PreviousSibling();
  while (prev_section) {
    if (prev_section->IsTableSection() && prev_section != head_ &&
        prev_section != foot_ &&
        (skip_empty_sections == kDoNotSkipEmptySections ||
         To<LayoutTableSection>(prev_section)->NumRows()))
      break;
    prev_section = prev_section->PreviousSibling();
  }
  if (!prev_section && head_ &&
      (skip_empty_sections == kDoNotSkipEmptySections || head_->NumRows()))
    prev_section = head_;
  return To<LayoutTableSection>(prev_section);
}

LayoutTableSection* LayoutTable::SectionBelow(
    const LayoutTableSection* section,
    SkipEmptySectionsValue skip_empty_sections) const {
  RecalcSectionsIfNeeded();

  if (section == foot_)
    return nullptr;

  LayoutObject* next_section =
      section == head_ ? FirstChild() : section->NextSibling();
  while (next_section) {
    if (next_section->IsTableSection() && next_section != head_ &&
        next_section != foot_ &&
        (skip_empty_sections == kDoNotSkipEmptySections ||
         To<LayoutTableSection>(next_section)->NumRows()))
      break;
    next_section = next_section->NextSibling();
  }
  if (!next_section && foot_ &&
      (skip_empty_sections == kDoNotSkipEmptySections || foot_->NumRows()))
    next_section = foot_;
  return To<LayoutTableSection>(next_section);
}

LayoutTableSection* LayoutTable::BottomSection() const {
  RecalcSectionsIfNeeded();

  if (foot_)
    return foot_;

  if (head_ && !first_body_)
    return head_;

  for (LayoutObject* child = LastChild(); child;
       child = child->PreviousSibling()) {
    if (child == head_)
      continue;
    if (child->IsTableSection())
      return To<LayoutTableSection>(child);
  }

  return nullptr;
}

LayoutTableCell* LayoutTable::CellAbove(const LayoutTableCell& cell) const {
  RecalcSectionsIfNeeded();

  // Find the section and row to look in
  unsigned r = cell.RowIndex();
  LayoutTableSection* section = nullptr;
  unsigned r_above = 0;
  if (r > 0) {
    // cell is not in the first row, so use the above row in its own section
    section = cell.Section();
    r_above = r - 1;
  } else {
    section = SectionAbove(cell.Section(), kSkipEmptySections);
    if (section) {
      DCHECK(section->NumRows());
      r_above = section->NumRows() - 1;
    }
  }

  // Look up the cell in the section's grid, which requires effective col index
  if (section) {
    unsigned eff_col =
        AbsoluteColumnToEffectiveColumn(cell.AbsoluteColumnIndex());
    return section->PrimaryCellAt(r_above, eff_col);
  }
  return nullptr;
}

LayoutTableCell* LayoutTable::CellBelow(const LayoutTableCell& cell) const {
  RecalcSectionsIfNeeded();

  // Find the section and row to look in
  unsigned r = cell.RowIndex() + cell.ResolvedRowSpan() - 1;
  LayoutTableSection* section = nullptr;
  unsigned r_below = 0;
  if (r < cell.Section()->NumRows() - 1) {
    // The cell is not in the last row, so use the next row in the section.
    section = cell.Section();
    r_below = r + 1;
  } else {
    section = SectionBelow(cell.Section(), kSkipEmptySections);
    if (section)
      r_below = 0;
  }

  // Look up the cell in the section's grid, which requires effective col index
  if (section) {
    unsigned eff_col =
        AbsoluteColumnToEffectiveColumn(cell.AbsoluteColumnIndex());
    return section->PrimaryCellAt(r_below, eff_col);
  }
  return nullptr;
}

LayoutTableCell* LayoutTable::CellPreceding(const LayoutTableCell& cell) const {
  RecalcSectionsIfNeeded();

  LayoutTableSection* section = cell.Section();
  unsigned eff_col =
      AbsoluteColumnToEffectiveColumn(cell.AbsoluteColumnIndex());
  if (!eff_col)
    return nullptr;

  // If we hit a colspan back up to a real cell.
  return section->PrimaryCellAt(cell.RowIndex(), eff_col - 1);
}

LayoutTableCell* LayoutTable::CellFollowing(const LayoutTableCell& cell) const {
  RecalcSectionsIfNeeded();

  unsigned eff_col = AbsoluteColumnToEffectiveColumn(
      cell.AbsoluteColumnIndex() + cell.ColSpan());
  return cell.Section()->PrimaryCellAt(cell.RowIndex(), eff_col);
}

LayoutUnit LayoutTable::BaselinePosition(
    FontBaseline baseline_type,
    bool first_line,
    LineDirectionMode direction,
    LinePositionMode line_position_mode) const {
  DCHECK_EQ(line_position_mode, kPositionOnContainingLine);
  LayoutUnit baseline = FirstLineBoxBaseline();
  if (baseline != -1) {
    if (IsInline())
      return BeforeMarginInLineDirection(direction) + baseline;
    return baseline;
  }

  return LayoutBox::BaselinePosition(baseline_type, first_line, direction,
                                     line_position_mode);
}

LayoutUnit LayoutTable::InlineBlockBaseline(LineDirectionMode) const {
  // Tables are skipped when computing an inline-block's baseline.
  return LayoutUnit(-1);
}

LayoutUnit LayoutTable::FirstLineBoxBaseline() const {
  // The baseline of a 'table' is the same as the 'inline-table' baseline per
  // CSS 3 Flexbox (CSS 2.1 doesn't define the baseline of a 'table' only an
  // 'inline-table'). This is also needed to properly determine the baseline of
  // a cell if it has a table child.

  if (IsWritingModeRoot() || ShouldApplyLayoutContainment())
    return LayoutUnit(-1);

  RecalcSectionsIfNeeded();

  const LayoutTableSection* top_non_empty_section = TopNonEmptySection();
  if (!top_non_empty_section)
    return LayoutUnit(-1);

  LayoutUnit baseline = top_non_empty_section->FirstLineBoxBaseline();
  if (baseline >= 0)
    return top_non_empty_section->LogicalTop() + baseline;

  // FF, Presto and IE use the top of the section as the baseline if its first
  // row is empty of cells or content.
  // The baseline of an empty row isn't specified by CSS 2.1.
  if (top_non_empty_section->FirstRow() &&
      !top_non_empty_section->FirstRow()->FirstCell())
    return top_non_empty_section->LogicalTop();

  return LayoutUnit(-1);
}

PhysicalRect LayoutTable::OverflowClipRect(
    const PhysicalOffset& location,
    OverlayScrollbarClipBehavior overlay_scrollbar_clip_behavior) const {
  if (ShouldCollapseBorders()) {
    // Though the outer halves of the collapsed borders are considered as the
    // the border area of the table by means of the box model, they are actually
    // contents of the table and should not be clipped off. The overflow clip
    // rect is BorderBoxRect() + location.
    return PhysicalRect(location, Size());
  }

  PhysicalRect rect =
      LayoutBlock::OverflowClipRect(location, overlay_scrollbar_clip_behavior);

  // If we have a caption, expand the clip to include the caption.
  // FIXME: Technically this is wrong, but it's virtually impossible to fix this
  // for real until captions have been re-written.
  // FIXME: This code assumes (like all our other caption code) that only
  // top/bottom are supported.  When we actually support left/right and stop
  // mapping them to top/bottom, we might have to hack this code first
  // (depending on what order we do these bug fixes in).
  if (!captions_.IsEmpty()) {
    if (StyleRef().IsHorizontalWritingMode()) {
      rect.size.height = Size().Height();
      rect.offset.top = location.top;
    } else {
      rect.size.width = Size().Width();
      rect.offset.left = location.left;
    }
  }

  return rect;
}

bool LayoutTable::NodeAtPoint(HitTestResult& result,
                              const HitTestLocation& hit_test_location,
                              const PhysicalOffset& accumulated_offset,
                              HitTestAction action) {
  // Check kids first.
  bool skip_children = (result.GetHitTestRequest().GetStopNode() == this);
  if (!skip_children &&
      (!HasOverflowClip() ||
       hit_test_location.Intersects(OverflowClipRect(accumulated_offset)))) {
    for (LayoutObject* child = LastChild(); child;
         child = child->PreviousSibling()) {
      if (child->IsBox() && !ToLayoutBox(child)->HasSelfPaintingLayer() &&
          (child->IsTableSection() || child->IsTableCaption())) {
        PhysicalOffset child_accumulated_offset =
            accumulated_offset + ToLayoutBox(child)->PhysicalLocation(this);
        if (child->NodeAtPoint(result, hit_test_location,
                               child_accumulated_offset, action)) {
          UpdateHitTestResult(result,
                              hit_test_location.Point() - accumulated_offset);
          return true;
        }
      }
    }
  }

  // Check our bounds next.
  PhysicalRect bounds_rect(accumulated_offset, Size());
  if (VisibleToHitTestRequest(result.GetHitTestRequest()) &&
      (action == kHitTestBlockBackground ||
       action == kHitTestChildBlockBackground) &&
      hit_test_location.Intersects(bounds_rect)) {
    UpdateHitTestResult(result, hit_test_location.Point() - accumulated_offset);
    if (result.AddNodeToListBasedTestResult(GetNode(), hit_test_location,
                                            bounds_rect) == kStopHitTesting)
      return true;
  }

  return false;
}

LayoutTable* LayoutTable::CreateAnonymousWithParent(
    const LayoutObject* parent) {
  scoped_refptr<ComputedStyle> new_style =
      ComputedStyle::CreateAnonymousStyleWithDisplay(
          parent->StyleRef(),
          parent->IsLayoutInline() ? EDisplay::kInlineTable : EDisplay::kTable);
  LayoutTable* new_table = new LayoutTable(nullptr);
  new_table->SetDocumentForAnonymous(&parent->GetDocument());
  new_table->SetStyle(std::move(new_style));
  return new_table;
}

void LayoutTable::EnsureIsReadyForPaintInvalidation() {
  LayoutBlock::EnsureIsReadyForPaintInvalidation();

  if (collapsed_borders_valid_)
    return;

  collapsed_borders_valid_ = true;
  has_collapsed_borders_ = false;
  needs_adjust_collapsed_border_joints_ = false;
  should_paint_all_collapsed_borders_ = false;
  if (!ShouldCollapseBorders())
    return;

  CollapsedBorderValue first_border;
  for (auto* section = TopSection(); section; section = SectionBelow(section)) {
    bool section_may_be_composited = section->IsPaintInvalidationContainer();
    for (auto* row = section->FirstRow(); row; row = row->NextRow()) {
      for (auto* cell = row->FirstCell(); cell; cell = cell->NextCell()) {
        DCHECK_EQ(cell->Table(), this);
        // Determine if there are any collapsed borders, and if so set
        // has_collapsed_borders_.
        const auto* values = cell->GetCollapsedBorderValues();
        if (!values)
          continue;
        has_collapsed_borders_ = true;

        // Determine if there are any differences other than color in any of the
        // borders of any cells (even if not adjacent), and if so set
        // needs_adjust_collapsed_border_joints_.
        if (needs_adjust_collapsed_border_joints_)
          continue;
        for (int i = 0; i < 4; ++i) {
          const auto& border = values->Borders()[i];
          if (!first_border.Exists()) {
            first_border = border;
          } else if (!first_border.IsSameIgnoringColor(border)) {
            needs_adjust_collapsed_border_joints_ = true;
            break;
          }
        }
      }

      // Collapsed borders should always be painted on the table's backing.
      // If any row is not on the same composited layer as the table, the table
      // should paint all collapsed borders.
      if (has_collapsed_borders_ &&
          (section_may_be_composited || row->IsPaintInvalidationContainer())) {
        // Pass the row's paint invalidation flag to the table in case that the
        // flag was set for collapsed borders.
        if (row->ShouldDoFullPaintInvalidation())
          SetShouldDoFullPaintInvalidation(PaintInvalidationReason::kStyle);
        should_paint_all_collapsed_borders_ = true;
      }
    }
  }
}

void LayoutTable::InvalidatePaint(
    const PaintInvalidatorContext& context) const {
  TablePaintInvalidator(*this, context).InvalidatePaint();
}

LayoutUnit LayoutTable::PaddingTop() const {
  if (ShouldCollapseBorders())
    return LayoutUnit();

  // TODO(crbug.com/377847): The ToInt call should be removed when Table is
  // sub-pixel aware.
  return LayoutUnit(LayoutBlock::PaddingTop().ToInt());
}

LayoutUnit LayoutTable::PaddingBottom() const {
  if (ShouldCollapseBorders())
    return LayoutUnit();

  // TODO(crbug.com/377847): The ToInt call should be removed when Table is
  // sub-pixel aware.
  return LayoutUnit(LayoutBlock::PaddingBottom().ToInt());
}

LayoutUnit LayoutTable::PaddingLeft() const {
  if (ShouldCollapseBorders())
    return LayoutUnit();

  // TODO(crbug.com/377847): The ToInt call should be removed when Table is
  // sub-pixel aware.
  return LayoutUnit(LayoutBlock::PaddingLeft().ToInt());
}

LayoutUnit LayoutTable::PaddingRight() const {
  if (ShouldCollapseBorders())
    return LayoutUnit();

  // TODO(crbug.com/377847): The ToInt call should be removed when Table is
  // sub-pixel aware.
  return LayoutUnit(LayoutBlock::PaddingRight().ToInt());
}

void LayoutTable::UpdateCollapsedOuterBorders() const {
  if (collapsed_outer_borders_valid_)
    return;

  // Something needs our collapsed borders before we've calculated them. Return
  // the old ones.
  if (NeedsSectionRecalc())
    return;

  collapsed_outer_borders_valid_ = true;
  if (!ShouldCollapseBorders())
    return;

  collapsed_outer_border_start_ = 0;
  collapsed_outer_border_end_ = 0;
  collapsed_outer_border_before_ = 0;
  collapsed_outer_border_after_ = 0;
  collapsed_outer_border_start_overflow_ = 0;
  collapsed_outer_border_end_overflow_ = 0;

  const auto* top_section = TopNonEmptySection();
  if (!top_section)
    return;

  // The table's before outer border width is the maximum before outer border
  // widths of all cells in the first row. See the CSS 2.1 spec, section 17.6.2.
  unsigned top_cols = top_section->NumCols(0);
  for (unsigned col = 0; col < top_cols; ++col) {
    if (const auto* cell = top_section->PrimaryCellAt(0, col)) {
      collapsed_outer_border_before_ = std::max(
          collapsed_outer_border_before_, cell->CollapsedOuterBorderBefore());
    }
  }

  // The table's after outer border width is the maximum after outer border
  // widths of all cells in the last row. See the CSS 2.1 spec, section 17.6.2.
  const auto* bottom_section = BottomNonEmptySection();
  DCHECK(bottom_section);
  unsigned row = bottom_section->NumRows() - 1;
  unsigned bottom_cols = bottom_section->NumCols(row);
  for (unsigned col = 0; col < bottom_cols; ++col) {
    if (const auto* cell = bottom_section->PrimaryCellAt(row, col)) {
      collapsed_outer_border_after_ = std::max(
          collapsed_outer_border_after_, cell->CollapsedOuterBorderAfter());
    }
  }

  // The table's start and end outer border widths are the border outer widths
  // of the first and last cells in the first row. See the CSS 2.1 spec,
  // section 17.6.2.
  bool first_row = true;
  unsigned max_border_start = 0;
  unsigned max_border_end = 0;
  for (const auto* section = top_section; section;
       section = SectionBelow(section, kSkipEmptySections)) {
    for (const auto* row = section->FirstRow(); row; row = row->NextRow()) {
      if (const auto* cell = row->FirstCell()) {
        auto border_start = cell->CollapsedOuterBorderStart();
        if (first_row)
          collapsed_outer_border_start_ = border_start;
        max_border_start = std::max(max_border_start, border_start);
      }
      if (const auto* cell = row->LastCell()) {
        auto border_end = cell->CollapsedOuterBorderEnd();
        if (first_row)
          collapsed_outer_border_end_ = border_end;
        max_border_end = std::max(max_border_end, border_end);
      }
      first_row = false;
    }
  }

  // Record the overflows caused by wider collapsed borders of the first/last
  // cell in rows other than the first.
  collapsed_outer_border_start_overflow_ =
      max_border_start - collapsed_outer_border_start_;
  collapsed_outer_border_end_overflow_ =
      max_border_end - collapsed_outer_border_end_;
}

bool LayoutTable::PaintedOutputOfObjectHasNoEffectRegardlessOfSize() const {
  return LayoutBlock::PaintedOutputOfObjectHasNoEffectRegardlessOfSize() &&
         !should_paint_all_collapsed_borders_;
}

// LayoutNGTableCellInterface API
bool LayoutTable::IsFirstCell(const LayoutNGTableCellInterface& cell) const {
  const LayoutTableCell& layout_cell = *cell.ToLayoutTableCell();
  return !(CellPreceding(layout_cell) || CellAbove(layout_cell));
}

}  // namespace blink
