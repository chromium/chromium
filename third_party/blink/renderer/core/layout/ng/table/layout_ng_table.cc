// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table.h"

#include "third_party/blink/renderer/core/layout/layout_analyzer.h"
#include "third_party/blink/renderer/core/layout/layout_object_factory.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_caption.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_column.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_section.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_borders.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_layout_algorithm_helpers.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_layout_algorithm_utils.h"
#include "third_party/blink/renderer/core/paint/ng/ng_box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_table_painters.h"

namespace blink {

namespace {

inline bool NeedsTableSection(const LayoutObject& object) {
  // Return true if 'object' can't exist in an anonymous table without being
  // wrapped in a table section box.
  EDisplay display = object.StyleRef().Display();
  return display != EDisplay::kTableCaption &&
         display != EDisplay::kTableColumnGroup &&
         display != EDisplay::kTableColumn;
}

}  // namespace

LayoutNGTable::LayoutNGTable(Element* element)
    : LayoutNGMixin<LayoutBlock>(element) {}

wtf_size_t LayoutNGTable::ColumnCount() const {
  NOT_DESTROYED();
  const NGLayoutResult* cached_layout_result = GetCachedLayoutResult();
  if (!cached_layout_result)
    return 0;
  return cached_layout_result->TableColumnCount();
}

void LayoutNGTable::SetCachedTableBorders(
    scoped_refptr<const NGTableBorders> table_borders) {
  NOT_DESTROYED();
  cached_table_borders_ = std::move(table_borders);
}

void LayoutNGTable::InvalidateCachedTableBorders() {
  NOT_DESTROYED();
  // TODO(layout-dev) When cached borders are invalidated, we could do a
  // special kind of relayout where fragments can replace only TableBorders,
  // keep the geometry, and repaint.
  cached_table_borders_.reset();
}

const NGTableTypes::Columns* LayoutNGTable::GetCachedTableColumnConstraints() {
  NOT_DESTROYED();
  if (IsTableColumnsConstraintsDirty())
    cached_table_columns_.reset();
  return cached_table_columns_.get();
}

void LayoutNGTable::SetCachedTableColumnConstraints(
    scoped_refptr<const NGTableTypes::Columns> columns) {
  NOT_DESTROYED();
  cached_table_columns_ = std::move(columns);
  SetTableColumnConstraintDirty(false);
}

void LayoutNGTable::GridBordersChanged() {
  NOT_DESTROYED();
  InvalidateCachedTableBorders();
  // If borders change, table fragment must be regenerated.
  if (StyleRef().BorderCollapse() == EBorderCollapse::kCollapse)
    SetNeedsLayout(layout_invalidation_reason::kTableChanged);
}

void LayoutNGTable::TableGridStructureChanged() {
  NOT_DESTROYED();
  InvalidateCachedTableBorders();
}

void LayoutNGTable::UpdateBlockLayout(bool relayout_children) {
  NOT_DESTROYED();
  LayoutAnalyzer::BlockScope analyzer(*this);

  if (IsOutOfFlowPositioned()) {
    UpdateOutOfFlowBlockLayout();
    return;
  }
  UpdateInFlowBlockLayout();
}

void LayoutNGTable::AddChild(LayoutObject* child, LayoutObject* before_child) {
  NOT_DESTROYED();
  TableGridStructureChanged();
  bool wrap_in_anonymous_section = !child->IsTableCaption() &&
                                   !child->IsLayoutTableCol() &&
                                   !child->IsTableSection();

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
    LayoutNGTableSection* section =
        DynamicTo<LayoutNGTableSection>(before_child->PreviousSibling());
    if (section && section->IsAnonymous()) {
      section->AddChild(child);
      return;
    }
  }

  LayoutObject* last_box = before_child;
  while (last_box && last_box->Parent()->IsAnonymous() &&
         !last_box->IsTableSection() && NeedsTableSection(*last_box))
    last_box = last_box->Parent();
  if (last_box && last_box->IsAnonymous() && last_box->IsTablePart() &&
      !IsAfterContent(last_box)) {
    if (before_child == last_box)
      before_child = last_box->SlowFirstChild();
    last_box->AddChild(child, before_child);
    return;
  }

  if (before_child && !before_child->IsTableSection() &&
      NeedsTableSection(*before_child))
    before_child = nullptr;

  LayoutBox* section =
      LayoutObjectFactory::CreateAnonymousTableSectionWithParent(*this);
  AddChild(section, before_child);
  section->AddChild(child);
}

void LayoutNGTable::RemoveChild(LayoutObject* child) {
  NOT_DESTROYED();
  TableGridStructureChanged();
  LayoutNGMixin<LayoutBlock>::RemoveChild(child);
}

void LayoutNGTable::StyleDidChange(StyleDifference diff,
                                   const ComputedStyle* old_style) {
  NOT_DESTROYED();
  // StyleDifference handles changes in table-layout, border-spacing.
  if (old_style) {
    bool borders_changed = !old_style->BorderVisuallyEqual(StyleRef()) ||
                           (diff.TextDecorationOrColorChanged() &&
                            StyleRef().HasBorderColorReferencingCurrentColor());
    bool collapse_changed =
        StyleRef().BorderCollapse() != old_style->BorderCollapse();
    if (borders_changed || collapse_changed)
      GridBordersChanged();
  }
  LayoutNGMixin<LayoutBlock>::StyleDidChange(diff, old_style);
}

LayoutBox* LayoutNGTable::CreateAnonymousBoxWithSameTypeAs(
    const LayoutObject* parent) const {
  NOT_DESTROYED();
  return LayoutObjectFactory::CreateAnonymousTableWithParent(*parent);
}

PhysicalRect LayoutNGTable::OverflowClipRect(
    const PhysicalOffset& location,
    OverlayScrollbarClipBehavior overlay_scrollbar_clip_behavior) const {
  NOT_DESTROYED();
  PhysicalRect clip_rect;
  if (StyleRef().BorderCollapse() == EBorderCollapse::kCollapse) {
    clip_rect = PhysicalRect(location, Size());
    const auto overflow_clip = GetOverflowClipAxes();
    IntRect infinite_rect = PhysicalRect::InfiniteIntRect();
    if ((overflow_clip & kOverflowClipX) == kNoOverflowClip) {
      clip_rect.offset.left = LayoutUnit(infinite_rect.X());
      clip_rect.size.width = LayoutUnit(infinite_rect.Width());
    }
    if ((overflow_clip & kOverflowClipY) == kNoOverflowClip) {
      clip_rect.offset.top = LayoutUnit(infinite_rect.Y());
      clip_rect.size.height = LayoutUnit(infinite_rect.Height());
    }
  } else {
    clip_rect = LayoutNGMixin<LayoutBlock>::OverflowClipRect(
        location, overlay_scrollbar_clip_behavior);
  }
  // TODO(1142929)
  // We cannot handle table hidden overflow with captions correctly.
  // Correct handling would clip table grid content to grid content rect,
  // but not clip the captions.
  // Since we are not generating table's grid fragment, this is not
  // possible.
  // The current solution is to not clip if we have captions.
  // Maybe a fix is to do an additional clip in table painter?
  const LayoutBox* child = FirstChildBox();
  while (child) {
    if (child->IsTableCaption()) {
      // If there are captions, we cannot clip to content box.
      clip_rect.Unite(PhysicalRect(location, Size()));
      break;
    }
    child = child->NextSiblingBox();
  }
  return clip_rect;
}

void LayoutNGTable::AddVisualEffectOverflow() {
  NOT_DESTROYED();
  // TODO(1061423) Fragment painting: need a correct fragment.
  if (const NGPhysicalBoxFragment* fragment = GetPhysicalFragment(0)) {
    DCHECK_EQ(PhysicalFragmentCount(), 1u);
    // Table's collapsed borders contribute to visual overflow.
    // In the inline direction, table's border box does not include
    // visual border width (largest border), but does include
    // layout border width (border of first cell).
    // Expands border box to include visual border width.
    if (const NGTableBorders* collapsed_borders =
            fragment->TableCollapsedBorders()) {
      PhysicalRect borders_overflow = PhysicalBorderBoxRect();
      NGBoxStrut table_borders = collapsed_borders->TableBorder();
      auto visual_inline_strut =
          collapsed_borders->GetCollapsedBorderVisualInlineStrut();
      // Expand by difference between visual and layout border width.
      table_borders.inline_start =
          visual_inline_strut.first - table_borders.inline_start;
      table_borders.inline_end =
          visual_inline_strut.second - table_borders.inline_end;
      table_borders.block_start = LayoutUnit();
      table_borders.block_end = LayoutUnit();
      borders_overflow.Expand(
          table_borders.ConvertToPhysical(StyleRef().GetWritingDirection()));
      AddSelfVisualOverflow(borders_overflow);
    }
  }
  LayoutNGMixin<LayoutBlock>::AddVisualEffectOverflow();
}

void LayoutNGTable::Paint(const PaintInfo& paint_info) const {
  NOT_DESTROYED();
  DCHECK_EQ(PhysicalFragmentCount(), 1u);
  NGBoxFragmentPainter(*LayoutNGMixin<LayoutBlock>::GetPhysicalFragment(0))
      .Paint(paint_info);
}

LayoutUnit LayoutNGTable::BorderLeft() const {
  NOT_DESTROYED();
  // DCHECK(cached_table_borders_.get())
  // ScrollAnchoring fails this DCHECK.
  if (ShouldCollapseBorders() && cached_table_borders_.get()) {
    return cached_table_borders_->TableBorder()
        .ConvertToPhysical(Style()->GetWritingDirection())
        .left;
  }
  return LayoutNGMixin<LayoutBlock>::BorderLeft();
}

LayoutUnit LayoutNGTable::BorderRight() const {
  NOT_DESTROYED();
  // DCHECK(cached_table_borders_.get())
  // ScrollAnchoring fails this DCHECK.
  if (ShouldCollapseBorders() && cached_table_borders_.get()) {
    return cached_table_borders_->TableBorder()
        .ConvertToPhysical(Style()->GetWritingDirection())
        .right;
  }
  return LayoutNGMixin<LayoutBlock>::BorderRight();
}

LayoutUnit LayoutNGTable::BorderTop() const {
  NOT_DESTROYED();
  // DCHECK(cached_table_borders_.get())
  // ScrollAnchoring fails this DCHECK.
  if (ShouldCollapseBorders() && cached_table_borders_.get()) {
    return cached_table_borders_->TableBorder()
        .ConvertToPhysical(Style()->GetWritingDirection())
        .top;
  }
  return LayoutNGMixin<LayoutBlock>::BorderTop();
}

LayoutUnit LayoutNGTable::BorderBottom() const {
  NOT_DESTROYED();
  // DCHECK(cached_table_borders_.get())
  // ScrollAnchoring fails this DCHECK.
  if (ShouldCollapseBorders() && cached_table_borders_.get()) {
    return cached_table_borders_->TableBorder()
        .ConvertToPhysical(Style()->GetWritingDirection())
        .bottom;
  }
  return LayoutNGMixin<LayoutBlock>::BorderBottom();
}

LayoutUnit LayoutNGTable::PaddingTop() const {
  NOT_DESTROYED();
  if (ShouldCollapseBorders())
    return LayoutUnit();
  return LayoutNGMixin<LayoutBlock>::PaddingTop();
}

LayoutUnit LayoutNGTable::PaddingBottom() const {
  NOT_DESTROYED();
  if (ShouldCollapseBorders())
    return LayoutUnit();
  return LayoutNGMixin<LayoutBlock>::PaddingBottom();
}

LayoutUnit LayoutNGTable::PaddingLeft() const {
  NOT_DESTROYED();
  if (ShouldCollapseBorders())
    return LayoutUnit();
  return LayoutNGMixin<LayoutBlock>::PaddingLeft();
}

LayoutUnit LayoutNGTable::PaddingRight() const {
  NOT_DESTROYED();
  if (ShouldCollapseBorders())
    return LayoutUnit();
  return LayoutNGMixin<LayoutBlock>::PaddingRight();
}

LayoutRectOutsets LayoutNGTable::BorderBoxOutsets() const {
  NOT_DESTROYED();
  // DCHECK(cached_table_borders_.get())
  // ScrollAnchoring fails this DCHECK.
  if (PhysicalFragmentCount() > 0) {
    return GetPhysicalFragment(0)->Borders().ToLayoutRectOutsets();
  }
  NOTREACHED();
  return LayoutRectOutsets();
}

bool LayoutNGTable::IsFirstCell(const LayoutNGTableCellInterface& cell) const {
  NOT_DESTROYED();
  const LayoutNGTableRowInterface* row = cell.RowInterface();
  if (row->FirstCellInterface() != &cell)
    return false;
  const LayoutNGTableSectionInterface* section = row->SectionInterface();
  if (section->FirstRowInterface() != row)
    return false;
  NGTableGroupedChildren grouped_children(
      NGBlockNode(const_cast<LayoutNGTable*>(this)));
  auto first_section = grouped_children.begin();
  return first_section != grouped_children.end() &&
         ToInterface<LayoutNGTableSectionInterface>(
             (*first_section).GetLayoutBox()) == section;
}

// Only called from AXLayoutObject::IsDataTable()
LayoutNGTableSectionInterface* LayoutNGTable::FirstBodyInterface() const {
  NOT_DESTROYED();
  for (LayoutObject* child = FirstChild(); child;
       child = child->NextSibling()) {
    if (child->StyleRef().Display() == EDisplay::kTableRowGroup)
      return ToInterface<LayoutNGTableSectionInterface>(child);
  }
  return nullptr;
}

// Called from many AXLayoutObject methods.
LayoutNGTableSectionInterface* LayoutNGTable::TopSectionInterface() const {
  NOT_DESTROYED();
  NGTableGroupedChildren grouped_children(
      NGBlockNode(const_cast<LayoutNGTable*>(this)));
  auto first_section = grouped_children.begin();
  if (first_section != grouped_children.end()) {
    return ToInterface<LayoutNGTableSectionInterface>(
        (*first_section).GetLayoutBox());
  }
  return nullptr;
}

// Called from many AXLayoutObject methods.
LayoutNGTableSectionInterface* LayoutNGTable::SectionBelowInterface(
    const LayoutNGTableSectionInterface* target,
    SkipEmptySectionsValue skip) const {
  NOT_DESTROYED();
  NGTableGroupedChildren grouped_children(
      NGBlockNode(const_cast<LayoutNGTable*>(this)));
  bool found = false;
  for (NGBlockNode section : grouped_children) {
    if (found &&
        ((skip == kDoNotSkipEmptySections) || (!section.IsEmptyTableSection())))
      return To<LayoutNGTableSection>(section.GetLayoutBox());
    if (target == To<LayoutNGTableSection>(section.GetLayoutBox())
                      ->ToLayoutNGTableSectionInterface())
      found = true;
  }
  return nullptr;
}

}  // namespace blink
