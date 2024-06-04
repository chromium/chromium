// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/table/table_node.h"

#include "third_party/blink/renderer/core/layout/table/layout_table.h"
#include "third_party/blink/renderer/core/layout/table/table_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/table/table_layout_utils.h"

namespace blink {

const TableBorders* TableNode::GetTableBorders() const {
  auto* layout_table = To<LayoutTable>(box_.Get());
  const TableBorders* table_borders = layout_table->GetCachedTableBorders();
  if (!table_borders) {
    table_borders = TableBorders::ComputeTableBorders(*this);
    layout_table->SetCachedTableBorders(table_borders);
  } else {
#if DCHECK_IS_ON()
    // TODO(crbug.com/1191742) remove these DCHECKs as soon as bug is found.
    auto* duplicate_table_borders = TableBorders::ComputeTableBorders(*this);
    DCHECK(*duplicate_table_borders == *table_borders);
#endif
  }
  return table_borders;
}

const BoxStrut& TableNode::GetTableBordersStrut() const {
  return GetTableBorders()->TableBorder();
}

scoped_refptr<const TableTypes::Columns> TableNode::GetColumnConstraints(
    const TableGroupedChildren& grouped_children,
    const BoxStrut& border_padding) const {
  auto* layout_table = To<LayoutTable>(box_.Get());
  scoped_refptr<const TableTypes::Columns> column_constraints =
      layout_table->GetCachedTableColumnConstraints();
  if (!column_constraints) {
    column_constraints = ComputeColumnConstraints(
        *this, grouped_children, *GetTableBorders(), border_padding);
    layout_table->SetCachedTableColumnConstraints(column_constraints.get());
  }
  return column_constraints;
}

LayoutUnit TableNode::ComputeTableInlineSize(
    const ConstraintSpace& space,
    const BoxStrut& border_padding) const {
  return TableLayoutAlgorithm::ComputeTableInlineSize(*this, space,
                                                      border_padding);
}

LayoutUnit TableNode::ComputeCaptionBlockSize(
    const ConstraintSpace& space) const {
  FragmentGeometry geometry =
      CalculateInitialFragmentGeometry(space, *this, /* break_token */ nullptr);
  LayoutAlgorithmParams params(*this, geometry, space);
  TableLayoutAlgorithm algorithm(params);
  return algorithm.ComputeCaptionBlockSize();
}

bool TableNode::AllowColumnPercentages(bool is_layout_pass) const {
  if (Style().LogicalWidth().HasMaxContent()) {
    return false;
  }
  if (is_layout_pass)
    return true;
  // TODO(layout-dev): This function breaks the rule of "no tree-walks".
  // However for this specific case it adds a lot of overhead for little gain.
  // In the future, we could have a bit on a LayoutObject which indicates if we
  // should allow column percentages, and maintain this when adding/removing
  // from the tree.
  const LayoutBlock* block = box_->ContainingBlock();
  while (!block->IsLayoutView()) {
    if (block->IsTableCell() || block->IsFlexibleBox() ||
        block->IsLayoutGrid()) {
      return false;
    }

    block = block->ContainingBlock();
  }
  return true;
}

}  // namespace blink
