// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_NG_TABLE_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_NG_TABLE_NODE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"

#include "third_party/blink/renderer/core/layout/ng/table/ng_table_borders.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_layout_algorithm_types.h"

namespace blink {

// Table specific extensions to NGBlockNode.
class CORE_EXPORT NGTableNode final : public NGBlockNode {
 public:
  explicit NGTableNode(LayoutBox* box) : NGBlockNode(box) {}

  const NGBoxStrut& GetTableBordersStrut() const;

  scoped_refptr<const NGTableBorders> GetTableBorders() const;

  LayoutUnit ComputeCaptionBlockSize(const NGConstraintSpace& space) const;

  scoped_refptr<const NGTableTypes::Columns> GetColumnConstraints(
      const NGTableGroupedChildren&,
      const NGBoxStrut& border_padding) const;

  LayoutUnit ComputeTableInlineSize(const NGConstraintSpace&,
                                    const NGBoxStrut& border_padding) const;

  // Tables are special in that their max intrinsic-size can be "infinite"
  // (they should consume as much space as possible). However a lot of layout
  // modes (flex/grid) don't deal well with "infinite" max intrinsic-size, so
  // we disable this behaviour whenever we are an arbitrary descendant of one
  // of these layout modes.
  //
  // TODO(layout-dev): This isn't ideal, as we may have a fixed inline-size
  // parent where an "infinite" size would be fine.
  bool AllowColumnPercentages(bool is_layout_pass) const;
};

template <>
struct DowncastTraits<NGTableNode> {
  static bool AllowFrom(const NGLayoutInputNode& node) {
    return node.IsNGTable();
  }
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_NG_TABLE_NODE_H_
