// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_layout_input_node.h"

#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/intrinsic_sizing_info.h"
#include "third_party/blink/renderer/core/layout/layout_replaced.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/min_max_sizes.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_cell.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_column.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_section.h"
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

bool NGLayoutInputNode::IsSlider() const {
  if (const auto* input = DynamicTo<HTMLInputElement>(box_->GetNode()))
    return input->type() == input_type_names::kRange;
  return false;
}

bool NGLayoutInputNode::IsEmptyTableSection() const {
  return box_->IsTableSection() && To<LayoutNGTableSection>(box_)->IsEmpty();
}

wtf_size_t NGLayoutInputNode::TableColumnSpan() const {
  DCHECK(IsTableCol() || IsTableColgroup());
  return To<LayoutNGTableColumn>(box_)->Span();
}

wtf_size_t NGLayoutInputNode::TableCellColspan() const {
  DCHECK(box_->IsTableCell());
  return To<LayoutNGTableCell>(box_)->ColSpan();
}

wtf_size_t NGLayoutInputNode::TableCellRowspan() const {
  DCHECK(box_->IsTableCell());
  return To<LayoutNGTableCell>(box_)->ComputedRowSpan();
}

MinMaxSizesResult NGLayoutInputNode::ComputeMinMaxSizes(
    WritingMode writing_mode,
    const MinMaxSizesInput& input,
    const NGConstraintSpace* space) const {
  if (auto* inline_node = DynamicTo<NGInlineNode>(this))
    return inline_node->ComputeMinMaxSizes(writing_mode, input, space);
  return To<NGBlockNode>(*this).ComputeMinMaxSizes(writing_mode, input, space);
}

void NGLayoutInputNode::IntrinsicSize(
    base::Optional<LayoutUnit>* computed_inline_size,
    base::Optional<LayoutUnit>* computed_block_size) const {
  DCHECK(IsReplaced());

  GetOverrideIntrinsicSize(computed_inline_size, computed_block_size);
  if (*computed_inline_size && *computed_block_size)
    return;

  IntrinsicSizingInfo legacy_sizing_info;

  ToLayoutReplaced(box_)->ComputeIntrinsicSizingInfo(legacy_sizing_info);
  if (!*computed_inline_size && legacy_sizing_info.has_width)
    *computed_inline_size = LayoutUnit(legacy_sizing_info.size.Width());
  if (!*computed_block_size && legacy_sizing_info.has_height)
    *computed_block_size = LayoutUnit(legacy_sizing_info.size.Height());
}

NGLayoutInputNode NGLayoutInputNode::NextSibling() const {
  auto* inline_node = DynamicTo<NGInlineNode>(this);
  return inline_node ? inline_node->NextSibling()
                     : To<NGBlockNode>(*this).NextSibling();
}

PhysicalSize NGLayoutInputNode::InitialContainingBlockSize() const {
  IntSize icb_size =
      GetDocument().GetLayoutView()->GetLayoutSize(kIncludeScrollbars);
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
  DLOG(INFO) << "\n" << string_builder.ToString().Utf8();
}
#endif

void NGLayoutInputNode::GetOverrideIntrinsicSize(
    base::Optional<LayoutUnit>* computed_inline_size,
    base::Optional<LayoutUnit>* computed_block_size) const {
  DCHECK(IsReplaced());

  LayoutUnit override_inline_size = OverrideIntrinsicContentInlineSize();
  if (override_inline_size != kIndefiniteSize) {
    *computed_inline_size = override_inline_size;
  } else {
    LayoutUnit default_inline_size = DefaultIntrinsicContentInlineSize();
    if (default_inline_size != kIndefiniteSize)
      *computed_inline_size = default_inline_size;
  }

  LayoutUnit override_block_size = OverrideIntrinsicContentBlockSize();
  if (override_block_size != kIndefiniteSize) {
    *computed_block_size = override_block_size;
  } else {
    LayoutUnit default_block_size = DefaultIntrinsicContentBlockSize();
    if (default_block_size != kIndefiniteSize)
      *computed_block_size = default_block_size;
  }

  if (ShouldApplySizeContainment()) {
    if (!*computed_inline_size)
      *computed_inline_size = LayoutUnit();
    if (!*computed_block_size)
      *computed_block_size = LayoutUnit();
  }
}

}  // namespace blink
