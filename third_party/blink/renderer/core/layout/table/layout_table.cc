// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/table/layout_table.h"

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/constraint_space.h"
#include "third_party/blink/renderer/core/layout/layout_result.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_caption.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_column.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_row.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_section.h"
#include "third_party/blink/renderer/core/layout/table/table_borders.h"
#include "third_party/blink/renderer/core/layout/table/table_layout_utils.h"
#include "third_party/blink/renderer/core/paint/box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/table_painters.h"
#include "third_party/blink/renderer/platform/geometry/infinite_int_rect.h"

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

LayoutTable::LayoutTable(Element* element) : LayoutBlock(element) {}

LayoutTable::~LayoutTable() = default;

void LayoutTable::Trace(Visitor* visitor) const {
  visitor->Trace(cached_table_borders_);
  LayoutBlock::Trace(visitor);
}

// https://drafts.csswg.org/css-tables-3/#fixup-algorithm
// 3.2. If the box’s parent is an inline, run-in, or ruby box (or any box that
// would perform inlinification of its children), then an inline-table box must
// be generated; otherwise it must be a table box.
bool LayoutTable::ShouldCreateInlineAnonymous(const LayoutObject& parent) {
  return parent.IsLayoutInline() || parent.IsRubyBase() || parent.IsRubyText();
}

LayoutTable* LayoutTable::CreateAnonymousWithParent(
    const LayoutObject& parent) {
  const ComputedStyle& parent_style = parent.StyleRef();
  const ComputedStyle* new_style =
      parent.GetDocument().GetStyleResolver().CreateAnonymousStyleWithDisplay(
          parent_style, ShouldCreateInlineAnonymous(parent)
                            ? EDisplay::kInlineTable
                            : EDisplay::kTable);
  auto* new_table = MakeGarbageCollected<LayoutTable>(nullptr);
  new_table->SetDocumentForAnonymous(&parent.GetDocument());
  new_table->SetStyle(new_style);
  return new_table;
}

bool LayoutTable::IsFirstCell(const LayoutTableCell& cell) const {
  NOT_DESTROYED();
  const LayoutTableRow* row = cell.Row();
  if (row->FirstCell() != &cell) {
    return false;
  }
  const LayoutTableSection* section = row->Section();
  if (section->FirstRow() != row) {
    return false;
  }
  TableGroupedChildren grouped_children(
      BlockNode(const_cast<LayoutTable*>(this)));
  auto first_section = grouped_children.begin();
  return first_section != grouped_children.end() &&
         (*first_section).GetLayoutBox() == section;
}

LayoutTableSection* LayoutTable::FirstSection() const {
  NOT_DESTROYED();
  TableGroupedChildren grouped_children(
      BlockNode(const_cast<LayoutTable*>(this)));
  auto first_section = grouped_children.begin();
  if (first_section != grouped_children.end()) {
    auto* section_object =
        To<LayoutTableSection>((*first_section).GetLayoutBox());
    if ((*first_section).IsEmptyTableSection()) {
      return NextSection(section_object);
    }
    return section_object;
  }
  return nullptr;
}

LayoutTableSection* LayoutTable::LastSection() const {
  NOT_DESTROYED();
  TableGroupedChildren grouped_children(
      BlockNode(const_cast<LayoutTable*>(this)));
  auto last_section = --grouped_children.end();
  if (last_section != grouped_children.end()) {
    auto* section_object =
        To<LayoutTableSection>((*last_section).GetLayoutBox());
    if ((*last_section).IsEmptyTableSection()) {
      return PreviousSection(section_object);
    }
    return section_object;
  }
  return nullptr;
}

LayoutTableSection* LayoutTable::NextSection(
    const LayoutTableSection* current) const {
  NOT_DESTROYED();
  TableGroupedChildren grouped_children(
      BlockNode(const_cast<LayoutTable*>(this)));
  bool found = false;
  for (BlockNode section : grouped_children) {
    if (found && !section.IsEmptyTableSection()) {
      return To<LayoutTableSection>(section.GetLayoutBox());
    }
    if (current == To<LayoutTableSection>(section.GetLayoutBox())) {
      found = true;
    }
  }
  return nullptr;
}

LayoutTableSection* LayoutTable::PreviousSection(
    const LayoutTableSection* current) const {
  NOT_DESTROYED();
  TableGroupedChildren grouped_children(
      BlockNode(const_cast<LayoutTable*>(this)));
  auto stop = --grouped_children.begin();
  bool found = false;
  for (auto it = --grouped_children.end(); it != stop; --it) {
    BlockNode section = *it;
    if (found && !section.IsEmptyTableSection()) {
      return To<LayoutTableSection>(section.GetLayoutBox());
    }
    if (current == To<LayoutTableSection>(section.GetLayoutBox())) {
      found = true;
    }
  }
  return nullptr;
}

wtf_size_t LayoutTable::ColumnCount() const {
  NOT_DESTROYED();
  const LayoutResult* cached_layout_result = GetCachedLayoutResult(nullptr);
  if (!cached_layout_result)
    return 0;
  return cached_layout_result->TableColumnCount();
}

void LayoutTable::SetCachedTableBorders(const TableBorders* table_borders) {
  NOT_DESTROYED();
  cached_table_borders_ = table_borders;
}

void LayoutTable::InvalidateCachedTableBorders() {
  NOT_DESTROYED();
  // TODO(layout-dev) When cached borders are invalidated, we could do a
  // special kind of relayout where fragments can replace only TableBorders,
  // keep the geometry, and repaint.
  cached_table_borders_ = nullptr;
}

const TableTypes::Columns* LayoutTable::GetCachedTableColumnConstraints() {
  NOT_DESTROYED();
  if (IsTableColumnsConstraintsDirty())
    cached_table_columns_.reset();
  return cached_table_columns_.get();
}

void LayoutTable::SetCachedTableColumnConstraints(
    scoped_refptr<const TableTypes::Columns> columns) {
  NOT_DESTROYED();
  cached_table_columns_ = std::move(columns);
  SetTableColumnConstraintDirty(false);
}

void LayoutTable::GridBordersChanged() {
  NOT_DESTROYED();
  InvalidateCachedTableBorders();
  if (StyleRef().BorderCollapse() == EBorderCollapse::kCollapse) {
    SetShouldDoFullPaintInvalidationWithoutLayoutChange(
        PaintInvalidationReason::kStyle);
    // If borders change, table fragment must be regenerated.
    SetNeedsLayoutAndIntrinsicWidthsRecalc(
        layout_invalidation_reason::kTableChanged);
  }
}

void LayoutTable::TableGridStructureChanged() {
  NOT_DESTROYED();
  // Callers must ensure table layout gets invalidated.
  InvalidateCachedTableBorders();
  if (StyleRef().BorderCollapse() == EBorderCollapse::kCollapse)
    SetShouldDoFullPaintInvalidation();
}

bool LayoutTable::HasBackgroundForPaint() const {
  NOT_DESTROYED();
  if (StyleRef().HasBackground())
    return true;
  DCHECK_GT(PhysicalFragmentCount(), 0u);
  const TableFragmentData::ColumnGeometries* column_geometries =
      GetPhysicalFragment(0)->TableColumnGeometries();
  if (column_geometries) {
    for (const auto& column_geometry : *column_geometries) {
      if (column_geometry.node.Style().HasBackground())
        return true;
    }
  }
  return false;
}

void LayoutTable::AddChild(LayoutObject* child, LayoutObject* before_child) {
  NOT_DESTROYED();
  TableGridStructureChanged();
  // Only TablesNG table parts are allowed.
  // TODO(1229581): Change this DCHECK to caption || column || section.
  DCHECK(child->IsLayoutNGObject() ||
         (!child->IsTableCaption() && !child->IsLayoutTableCol() &&
          !child->IsTableSection()));
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
    auto* section =
        DynamicTo<LayoutTableSection>(before_child->PreviousSibling());
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

  auto* section = LayoutTableSection::CreateAnonymousWithParent(*this);
  AddChild(section, before_child);
  section->AddChild(child);
}

void LayoutTable::RemoveChild(LayoutObject* child) {
  NOT_DESTROYED();
  TableGridStructureChanged();
  LayoutBlock::RemoveChild(child);
}

void LayoutTable::StyleDidChange(StyleDifference diff,
                                 const ComputedStyle* old_style) {
  NOT_DESTROYED();
  // StyleDifference handles changes in table-layout, border-spacing.
  if (old_style) {
    bool borders_changed =
        !old_style->BorderVisuallyEqual(StyleRef()) ||
        old_style->GetWritingDirection() != StyleRef().GetWritingDirection() ||
        old_style->IsFixedTableLayout() != StyleRef().IsFixedTableLayout() ||
        old_style->EmptyCells() != StyleRef().EmptyCells();
    bool collapse_changed =
        StyleRef().BorderCollapse() != old_style->BorderCollapse();
    if (borders_changed || collapse_changed)
      GridBordersChanged();
  }
  LayoutBlock::StyleDidChange(diff, old_style);
}

LayoutBox* LayoutTable::CreateAnonymousBoxWithSameTypeAs(
    const LayoutObject* parent) const {
  NOT_DESTROYED();
  return CreateAnonymousWithParent(*parent);
}

PhysicalRect LayoutTable::OverflowClipRect(
    const PhysicalOffset& location,
    OverlayScrollbarClipBehavior overlay_scrollbar_clip_behavior) const {
  NOT_DESTROYED();
  PhysicalRect clip_rect;
  if (StyleRef().BorderCollapse() == EBorderCollapse::kCollapse) {
    clip_rect = PhysicalRect(location, Size());
    const auto overflow_clip = GetOverflowClipAxes();
    gfx::Rect infinite_rect = InfiniteIntRect();
    if ((overflow_clip & kOverflowClipX) == kNoOverflowClip) {
      clip_rect.offset.left = LayoutUnit(infinite_rect.x());
      clip_rect.size.width = LayoutUnit(infinite_rect.width());
    }
    if ((overflow_clip & kOverflowClipY) == kNoOverflowClip) {
      clip_rect.offset.top = LayoutUnit(infinite_rect.y());
      clip_rect.size.height = LayoutUnit(infinite_rect.height());
    }
  } else {
    clip_rect = LayoutBlock::OverflowClipRect(location,
                                              overlay_scrollbar_clip_behavior);
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

LayoutUnit LayoutTable::BorderLeft() const {
  NOT_DESTROYED();
  // DCHECK(cached_table_borders_.get())
  // ScrollAnchoring fails this DCHECK.
  if (HasCollapsedBorders() && cached_table_borders_) {
    return cached_table_borders_->TableBorder()
        .ConvertToPhysical(Style()->GetWritingDirection())
        .left;
  }
  return LayoutBlock::BorderLeft();
}

LayoutUnit LayoutTable::BorderRight() const {
  NOT_DESTROYED();
  // DCHECK(cached_table_borders_.get())
  // ScrollAnchoring fails this DCHECK.
  if (HasCollapsedBorders() && cached_table_borders_) {
    return cached_table_borders_->TableBorder()
        .ConvertToPhysical(Style()->GetWritingDirection())
        .right;
  }
  return LayoutBlock::BorderRight();
}

LayoutUnit LayoutTable::BorderTop() const {
  NOT_DESTROYED();
  // DCHECK(cached_table_borders_.get())
  // ScrollAnchoring fails this DCHECK.
  if (HasCollapsedBorders() && cached_table_borders_) {
    return cached_table_borders_->TableBorder()
        .ConvertToPhysical(Style()->GetWritingDirection())
        .top;
  }
  return LayoutBlock::BorderTop();
}

LayoutUnit LayoutTable::BorderBottom() const {
  NOT_DESTROYED();
  // DCHECK(cached_table_borders_.get())
  // ScrollAnchoring fails this DCHECK.
  if (HasCollapsedBorders() && cached_table_borders_) {
    return cached_table_borders_->TableBorder()
        .ConvertToPhysical(Style()->GetWritingDirection())
        .bottom;
  }
  return LayoutBlock::BorderBottom();
}

LayoutUnit LayoutTable::PaddingTop() const {
  NOT_DESTROYED();
  return HasCollapsedBorders() ? LayoutUnit() : LayoutBlock::PaddingTop();
}

LayoutUnit LayoutTable::PaddingBottom() const {
  NOT_DESTROYED();
  return HasCollapsedBorders() ? LayoutUnit() : LayoutBlock::PaddingBottom();
}

LayoutUnit LayoutTable::PaddingLeft() const {
  NOT_DESTROYED();
  return HasCollapsedBorders() ? LayoutUnit() : LayoutBlock::PaddingLeft();
}

LayoutUnit LayoutTable::PaddingRight() const {
  NOT_DESTROYED();
  return HasCollapsedBorders() ? LayoutUnit() : LayoutBlock::PaddingRight();
}

// Effective column index is index of columns with mergeable
// columns skipped. Used in a11y.
unsigned LayoutTable::AbsoluteColumnToEffectiveColumn(
    unsigned absolute_column_index) const {
  NOT_DESTROYED();
  if (!cached_table_columns_) {
    NOTREACHED_IN_MIGRATION();
    return absolute_column_index;
  }
  unsigned effective_column_index = 0;
  unsigned column_count = cached_table_columns_.get()->data.size();
  for (unsigned current_column_index = 0; current_column_index < column_count;
       ++current_column_index) {
    if (current_column_index != 0 &&
        !cached_table_columns_.get()->data[current_column_index].is_mergeable)
      ++effective_column_index;
    if (current_column_index == absolute_column_index)
      return effective_column_index;
  }
  return effective_column_index;
}

unsigned LayoutTable::EffectiveColumnCount() const {
  NOT_DESTROYED();
  const wtf_size_t column_count = ColumnCount();
  if (column_count == 0) {
    return 0;
  }
  return AbsoluteColumnToEffectiveColumn(column_count - 1) + 1;
}

}  // namespace blink
