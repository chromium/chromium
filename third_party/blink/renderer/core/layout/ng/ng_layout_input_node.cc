// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_layout_input_node.h"

#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/intrinsic_sizing_info.h"
#include "third_party/blink/renderer/core/layout/layout_replaced.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/min_max_size.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {
namespace {

#if DCHECK_IS_ON()
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

  if (auto* block_node = DynamicTo<NGBlockNode>(node)) {
    NGLayoutInputNode first_child = block_node->FirstChild();
    for (NGLayoutInputNode node_runner = first_child; node_runner;
         node_runner = node_runner.NextSibling()) {
      string_builder->Append(indent_builder.ToString());
      AppendNodeToString(node_runner, string_builder, indent + 2);
    }
  }

  if (auto* inline_node = DynamicTo<NGInlineNode>(node)) {
    const auto& items = inline_node->ItemsData(false).items;
    for (const NGInlineItem& inline_item : items) {
      string_builder->Append(indent_builder.ToString());
      string_builder->Append(inline_item.ToString());
      string_builder->Append("\n");
    }
    NGLayoutInputNode next_sibling = inline_node->NextSibling();
    for (NGLayoutInputNode node_runner = next_sibling; node_runner;
         node_runner = node_runner.NextSibling()) {
      string_builder->Append(indent_builder.ToString());
      AppendNodeToString(node_runner, string_builder, indent + 2);
    }
  }
}
#endif

}  // namespace

MinMaxSize NGLayoutInputNode::ComputeMinMaxSize(
    WritingMode writing_mode,
    const MinMaxSizeInput& input,
    const NGConstraintSpace* space) {
  if (auto* inline_node = DynamicTo<NGInlineNode>(this))
    return inline_node->ComputeMinMaxSize(writing_mode, input, space);
  return To<NGBlockNode>(*this).ComputeMinMaxSize(writing_mode, input, space);
}

void NGLayoutInputNode::IntrinsicSize(
    base::Optional<LayoutUnit>* computed_inline_size,
    base::Optional<LayoutUnit>* computed_block_size,
    LogicalSize* aspect_ratio) const {
  DCHECK(IsReplaced());

  LayoutUnit override_inline_size = OverrideIntrinsicContentInlineSize();
  if (override_inline_size != kIndefiniteSize)
    *computed_inline_size = override_inline_size;

  LayoutUnit override_block_size = OverrideIntrinsicContentBlockSize();
  if (override_block_size != kIndefiniteSize)
    *computed_block_size = override_block_size;

  if (ShouldApplySizeContainment()) {
    if (!*computed_inline_size)
      *computed_inline_size = LayoutUnit();
    if (!*computed_block_size)
      *computed_block_size = LayoutUnit();
  }
  if (*computed_inline_size && *computed_block_size) {
    *aspect_ratio = LogicalSize(**computed_inline_size, **computed_block_size);
    return;
  }

  IntrinsicSizingInfo legacy_sizing_info;

  ToLayoutReplaced(box_)->ComputeIntrinsicSizingInfo(legacy_sizing_info);
  if (!*computed_inline_size && legacy_sizing_info.has_width)
    *computed_inline_size = LayoutUnit(legacy_sizing_info.size.Width());
  if (!*computed_block_size && legacy_sizing_info.has_height)
    *computed_block_size = LayoutUnit(legacy_sizing_info.size.Height());
  *aspect_ratio =
      LogicalSize(LayoutUnit(legacy_sizing_info.aspect_ratio.Width()),
                  LayoutUnit(legacy_sizing_info.aspect_ratio.Height()));
}

NGLayoutInputNode NGLayoutInputNode::NextSibling() {
  auto* inline_node = DynamicTo<NGInlineNode>(this);
  return inline_node ? inline_node->NextSibling()
                     : To<NGBlockNode>(*this).NextSibling();
}

PhysicalSize NGLayoutInputNode::InitialContainingBlockSize() const {
  IntSize icb_size =
      GetDocument().GetLayoutView()->GetLayoutSize(kExcludeScrollbars);
  return PhysicalSize(icb_size);
}

const NGPaintFragment* NGLayoutInputNode::PaintFragment() const {
  return GetLayoutBox()->PaintFragment();
}

String NGLayoutInputNode::ToString() const {
  auto* inline_node = DynamicTo<NGInlineNode>(this);
  return inline_node ? inline_node->ToString()
                     : To<NGBlockNode>(*this).ToString();
}

#if DCHECK_IS_ON()
void NGLayoutInputNode::ShowNodeTree() const {
  StringBuilder string_builder;
  string_builder.Append(".:: LayoutNG Node Tree ::.\n");
  AppendNodeToString(*this, &string_builder);
  fprintf(stderr, "%s\n", string_builder.ToString().Utf8().c_str());
}
#endif

}  // namespace blink
