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
#include "third_party/blink/renderer/core/style/border_edge.h"

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
  const NGLayoutResult* cached_layout_result = GetCachedLayoutResult();
  if (!cached_layout_result)
    return 0;
  return cached_layout_result->TableColumnCount();
}

void LayoutNGTable::SetCachedTableBorders(
    scoped_refptr<const NGTableBorders> table_borders) {
  cached_table_borders_ = std::move(table_borders);
}

void LayoutNGTable::InvalidateCachedTableBorders() {
  // When cached borders are invalidated, we could do a special kind of relayout
  // where fragments can replace only TableBorders, keep the geometry, and
  // repaint.
  cached_table_borders_.reset();
}

const NGTableTypes::Columns* LayoutNGTable::GetCachedTableColumnConstraints() {
  if (IsTableColumnsConstraintsDirty())
    cached_table_columns_.reset();
  return cached_table_columns_.get();
}

void LayoutNGTable::SetCachedTableColumnConstraints(
    scoped_refptr<const NGTableTypes::Columns> columns) {
  cached_table_columns_ = std::move(columns);
  SetTableColumnConstraintDirty(false);
}

void LayoutNGTable::GridBordersChanged() {
  InvalidateCachedTableBorders();
}

void LayoutNGTable::TableGridStructureChanged() {
  InvalidateCachedTableBorders();
}

void LayoutNGTable::UpdateBlockLayout(bool relayout_children) {
  LayoutAnalyzer::BlockScope analyzer(*this);

  if (IsOutOfFlowPositioned()) {
    UpdateOutOfFlowBlockLayout();
    return;
  }
  UpdateInFlowBlockLayout();
}

void LayoutNGTable::AddChild(LayoutObject* child, LayoutObject* before_child) {
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
  TableGridStructureChanged();
  LayoutNGMixin<LayoutBlock>::RemoveChild(child);
}

void LayoutNGTable::StyleDidChange(StyleDifference diff,
                                   const ComputedStyle* old_style) {
  // TODO(1115800) Make border invalidation more precise.
  if (NGTableBorders::HasBorder(old_style) ||
      NGTableBorders::HasBorder(Style())) {
    GridBordersChanged();
  }
  LayoutNGMixin<LayoutBlock>::StyleDidChange(diff, old_style);
}

LayoutBox* LayoutNGTable::CreateAnonymousBoxWithSameTypeAs(
    const LayoutObject* parent) const {
  return LayoutObjectFactory::CreateAnonymousTableWithParent(*parent);
}

bool LayoutNGTable::IsFirstCell(const LayoutNGTableCellInterface& cell) const {
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
  for (LayoutObject* child = FirstChild(); child;
       child = child->NextSibling()) {
    if (child->StyleRef().Display() == EDisplay::kTableRowGroup)
      return ToInterface<LayoutNGTableSectionInterface>(child);
  }
  return nullptr;
}

// Called from many AXLayoutObject methods.
LayoutNGTableSectionInterface* LayoutNGTable::TopSectionInterface() const {
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
