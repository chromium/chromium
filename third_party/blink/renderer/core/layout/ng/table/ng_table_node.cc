// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/table/ng_table_node.h"

#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_layout_algorithm_utils.h"

namespace blink {

scoped_refptr<const NGTableBorders> NGTableNode::GetTableBorders() const {
  LayoutNGTable* layout_table = To<LayoutNGTable>(box_);
  scoped_refptr<const NGTableBorders> table_borders =
      layout_table->GetCachedTableBorders();
  if (!table_borders) {
    table_borders = NGTableBorders::ComputeTableBorders(*this);
    layout_table->SetCachedTableBorders(table_borders.get());
  }
  return table_borders;
}

const NGBoxStrut& NGTableNode::GetTableBordersStrut() const {
  return GetTableBorders()->TableBorder();
}

scoped_refptr<const NGTableTypes::Columns> NGTableNode::GetColumnConstraints(
    const NGTableGroupedChildren& grouped_children,
    const NGBoxStrut& border_padding) const {
  LayoutNGTable* layout_table = To<LayoutNGTable>(box_);
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

}  // namespace blink
