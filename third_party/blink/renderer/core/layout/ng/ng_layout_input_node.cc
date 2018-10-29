// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_layout_input_node.h"

#include "third_party/blink/renderer/core/layout/intrinsic_sizing_info.h"
#include "third_party/blink/renderer/core/layout/layout_replaced.h"
#include "third_party/blink/renderer/core/layout/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/min_max_size.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_logical_size.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {
namespace {

#ifndef NDEBUG
void AppendNodeToString(NGLayoutInputNode node,
                        StringBuilder* string_builder,
                        unsigned indent = 2) {
  if (!node)
    return;
  DCHECK(string_builder);

  string_builder->Append(node.ToString());
  string_builder->Append("\n");

  StringBuilder indent_builder;
  for (unsigned i = 0; i < indent; i++)
    indent_builder.Append(" ");

  if (node.IsBlock()) {
    NGLayoutInputNode first_child = ToNGBlockNode(node).FirstChild();
    for (NGLayoutInputNode node_runner = first_child; node_runner;
         node_runner = node_runner.NextSibling()) {
      string_builder->Append(indent_builder.ToString());
      AppendNodeToString(node_runner, string_builder, indent + 2);
    }
  }

  if (node.IsInline()) {
    const auto& items = ToNGInlineNode(node).ItemsData(false).items;
    for (const NGInlineItem& inline_item : items) {
      string_builder->Append(indent_builder.ToString());
      string_builder->Append(inline_item.ToString());
      string_builder->Append("\n");
    }
    NGLayoutInputNode next_sibling = ToNGInlineNode(node).NextSibling();
    for (NGLayoutInputNode node_runner = next_sibling; node_runner;
         node_runner = node_runner.NextSibling()) {
      string_builder->Append(indent_builder.ToString());
      AppendNodeToString(node_runner, string_builder, indent + 2);
    }
  }
}
#endif

}  // namespace

scoped_refptr<NGLayoutResult> NGLayoutInputNode::Layout(
    const NGConstraintSpace& space,
    const NGBreakToken* break_token,
    NGInlineChildLayoutContext* context) {
  return IsInline() ? ToNGInlineNode(*this).Layout(space, break_token, context)
                    : ToNGBlockNode(*this).Layout(space, break_token);
}

MinMaxSize NGLayoutInputNode::ComputeMinMaxSize(
    WritingMode writing_mode,
    const MinMaxSizeInput& input,
    const NGConstraintSpace* space) {
  if (IsInline())
    return ToNGInlineNode(*this).ComputeMinMaxSize(writing_mode, input, space);
  return ToNGBlockNode(*this).ComputeMinMaxSize(writing_mode, input, space);
}

void NGLayoutInputNode::IntrinsicSize(
    NGLogicalSize* default_intrinsic_size,
    base::Optional<LayoutUnit>* computed_inline_size,
    base::Optional<LayoutUnit>* computed_block_size,
    NGLogicalSize* aspect_ratio) const {
  DCHECK(IsReplaced());

  LayoutSize box_intrinsic_size = box_->IntrinsicSize();
  // Transform to logical coordinates if needed.
  if (!Style().IsHorizontalWritingMode())
    box_intrinsic_size = box_intrinsic_size.TransposedSize();
  *default_intrinsic_size =
      NGLogicalSize(box_intrinsic_size.Width(), box_intrinsic_size.Height());

  IntrinsicSizingInfo legacy_sizing_info;

  ToLayoutReplaced(box_)->ComputeIntrinsicSizingInfo(legacy_sizing_info);
  if (legacy_sizing_info.has_width)
    *computed_inline_size = LayoutUnit(legacy_sizing_info.size.Width());
  if (legacy_sizing_info.has_height)
    *computed_block_size = LayoutUnit(legacy_sizing_info.size.Height());
  *aspect_ratio =
      NGLogicalSize(LayoutUnit(legacy_sizing_info.aspect_ratio.Width()),
                    LayoutUnit(legacy_sizing_info.aspect_ratio.Height()));
}

LayoutUnit NGLayoutInputNode::IntrinsicPaddingBlockStart() const {
  DCHECK(IsTableCell());
  return LayoutUnit(ToLayoutTableCell(box_)->IntrinsicPaddingBefore());
}

LayoutUnit NGLayoutInputNode::IntrinsicPaddingBlockEnd() const {
  DCHECK(IsTableCell());
  return LayoutUnit(ToLayoutTableCell(box_)->IntrinsicPaddingAfter());
}

NGLayoutInputNode NGLayoutInputNode::NextSibling() {
  return IsInline() ? ToNGInlineNode(*this).NextSibling()
                    : ToNGBlockNode(*this).NextSibling();
}

NGPhysicalSize NGLayoutInputNode::InitialContainingBlockSize() const {
  IntSize icb_size =
      GetDocument().GetLayoutView()->GetLayoutSize(kExcludeScrollbars);
  return NGPhysicalSize{LayoutUnit(icb_size.Width()),
                        LayoutUnit(icb_size.Height())};
}

String NGLayoutInputNode::ToString() const {
  return IsInline() ? ToNGInlineNode(*this).ToString()
                    : ToNGBlockNode(*this).ToString();
}

#ifndef NDEBUG
void NGLayoutInputNode::ShowNodeTree() const {
  StringBuilder string_builder;
  string_builder.Append(".:: LayoutNG Node Tree ::.\n");
  AppendNodeToString(*this, &string_builder);
  fprintf(stderr, "%s\n", string_builder.ToString().Utf8().data());
}
#endif

}  // namespace blink
