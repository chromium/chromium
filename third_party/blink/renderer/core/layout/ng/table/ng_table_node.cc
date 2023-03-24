// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/table/ng_table_node.h"

#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_layout_algorithm_utils.h"

namespace blink {

scoped_refptr<const NGTableBorders> NGTableNode::GetTableBorders() const {
  LayoutNGTable* layout_table = To<LayoutNGTable>(box_.Get());
  scoped_refptr<const NGTableBorders> table_borders =
      layout_table->GetCachedTableBorders();
  if (!table_borders) {
    table_borders = NGTableBorders::ComputeTableBorders(*this);
    layout_table->SetCachedTableBorders(table_borders.get());
  } else {
#if DCHECK_IS_ON()
    // TODO(crbug.com/1191742) remove these DCHECKs as soon as bug is found.
    auto duplicate_table_borders = NGTableBorders::ComputeTableBorders(*this);
    DCHECK(*duplicate_table_borders == *table_borders);
#endif
  }
  return table_borders;
}

const NGBoxStrut& NGTableNode::GetTableBordersStrut() const {
  return GetTableBorders()->TableBorder();
}

scoped_refptr<const NGTableTypes::Columns> NGTableNode::GetColumnConstraints(
    const NGTableGroupedChildren& grouped_children,
    const NGBoxStrut& border_padding) const {
  LayoutNGTable* layout_table = To<LayoutNGTable>(box_.Get());
  scoped_refptr<const NGTableTypes::Columns> column_constraints =
      layout_table->GetCachedTableColumnConstraints();
  if (!column_constraints) {
    column_constraints = NGTableAlgorithmUtils::ComputeColumnConstraints(
        *this, grouped_children, *GetTableBorders().get(), border_padding);
    layout_table->SetCachedTableColumnConstraints(column_constraints.get());
  }
  return column_constraints;
}

LayoutUnit NGTableNode::ComputeTableInlineSize(
    const NGConstraintSpace& space,
    const NGBoxStrut& border_padding) const {
  return NGTableLayoutAlgorithm::ComputeTableInlineSize(*this, space,
                                                        border_padding);
}

LayoutUnit NGTableNode::ComputeCaptionBlockSize(
    const NGConstraintSpace& space) const {
  LayoutUnit table_inline_size =
      CalculateInitialFragmentGeometry(space, *this, /* break_token */ nullptr)
          .border_box_size.inline_size;
  return NGTableLayoutAlgorithm::ComputeCaptionBlockSize(*this, space,
                                                         table_inline_size);
}

bool NGTableNode::AllowColumnPercentages(bool is_layout_pass) const {
  if (Style().LogicalWidth().IsMaxContent())
    return false;
  if (is_layout_pass)
    return true;
  // TODO(layout-dev): This function breaks the rule of "no tree-walks".
  // However for this specific case it adds a lot of overhead for little gain.
  // In the future, we could have a bit on a LayoutObject which indicates if we
  // should allow column percentages, and maintain this when adding/removing
  // from the tree.
  const LayoutBlock* block = box_->ContainingBlock();
  while (!block->IsLayoutView()) {
    if (block->IsTableCell() || block->IsFlexibleBoxIncludingNG() ||
        block->IsLayoutNGGrid()) {
      return false;
    }

    block = block->ContainingBlock();
  }
  return true;
}

}  // namespace blink
