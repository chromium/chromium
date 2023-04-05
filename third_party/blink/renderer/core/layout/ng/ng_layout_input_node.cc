// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_layout_input_node.h"

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_utils.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/intrinsic_sizing_info.h"
#include "third_party/blink/renderer/core/layout/layout_replaced.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/min_max_sizes.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_view.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_item.h"
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

bool NGLayoutInputNode::IsSliderThumb() const {
  return IsBlock() && blink::IsSliderThumb(GetDOMNode());
}

bool NGLayoutInputNode::IsSvgText() const {
  return box_ && box_->IsNGSVGText();
}

bool NGLayoutInputNode::IsEmptyTableSection() const {
  return box_->IsTableSection() &&
         To<LayoutNGTableSection>(box_.Get())->IsEmpty();
}

wtf_size_t NGLayoutInputNode::TableColumnSpan() const {
  DCHECK(IsTableCol() || IsTableColgroup());
  return To<LayoutNGTableColumn>(box_.Get())->Span();
}

wtf_size_t NGLayoutInputNode::TableCellColspan() const {
  DCHECK(box_->IsTableCell());
  return To<LayoutNGTableCell>(box_.Get())->ColSpan();
}

wtf_size_t NGLayoutInputNode::TableCellRowspan() const {
  DCHECK(box_->IsTableCell());
  return To<LayoutNGTableCell>(box_.Get())->ComputedRowSpan();
}

bool NGLayoutInputNode::IsTextControlPlaceholder() const {
  return IsBlock() && blink::IsTextControlPlaceholder(GetDOMNode());
}

bool NGLayoutInputNode::IsPaginatedRoot() const {
  if (!IsBlock())
    return false;
  const auto* view = DynamicTo<LayoutNGView>(box_.Get());
  return view && view->IsFragmentationContextRoot();
}

NGBlockNode NGLayoutInputNode::ListMarkerBlockNodeIfListItem() const {
  if (auto* list_item = DynamicTo<LayoutNGListItem>(box_.Get()))
    return NGBlockNode(DynamicTo<LayoutBox>(list_item->Marker()));
  return NGBlockNode(nullptr);
}

void NGLayoutInputNode::IntrinsicSize(
    absl::optional<LayoutUnit>* computed_inline_size,
    absl::optional<LayoutUnit>* computed_block_size) const {
  DCHECK(IsReplaced());

  GetOverrideIntrinsicSize(computed_inline_size, computed_block_size);
  if (*computed_inline_size && *computed_block_size)
    return;

  IntrinsicSizingInfo legacy_sizing_info;

  To<LayoutReplaced>(box_.Get())
      ->ComputeIntrinsicSizingInfo(legacy_sizing_info);
  if (!*computed_inline_size && legacy_sizing_info.has_width) {
    *computed_inline_size =
        LayoutUnit::FromFloatRound(legacy_sizing_info.size.width());
  }
  if (!*computed_block_size && legacy_sizing_info.has_height) {
    *computed_block_size =
        LayoutUnit::FromFloatRound(legacy_sizing_info.size.height());
  }
}

NGLayoutInputNode NGLayoutInputNode::NextSibling() const {
  auto* inline_node = DynamicTo<NGInlineNode>(this);
  return inline_node ? inline_node->NextSibling()
                     : To<NGBlockNode>(*this).NextSibling();
}

PhysicalSize NGLayoutInputNode::InitialContainingBlockSize() const {
  gfx::Size icb_size =
      GetDocument().GetLayoutView()->GetLayoutSize(kIncludeScrollbars);
  return PhysicalSize(icb_size);
}

String NGLayoutInputNode::ToString() const {
  auto* inline_node = DynamicTo<NGInlineNode>(this);
  return inline_node ? inline_node->ToString()
                     : To<NGBlockNode>(*this).ToString();
}

#if DCHECK_IS_ON()
void NGLayoutInputNode::ShowNodeTree() const {
  if (getenv("RUNNING_UNDER_RR")) {
    // Printing timestamps requires an IPC to get the local time, which
    // does not work in an rr replay session. Just disable timestamp printing
    // globally, since we don't need them. Affecting global state isn't a
    // problem because invoking this from a rr session creates a temporary
    // program environment that will be destroyed as soon as the invocation
    // completes.
    logging::SetLogItems(true, true, false, false);
  }

  StringBuilder string_builder;
  string_builder.Append(".:: LayoutNG Node Tree ::.\n");
  AppendNodeToString(*this, &string_builder);
  DLOG(INFO) << "\n" << string_builder.ToString().Utf8();
}
#endif

void NGLayoutInputNode::GetOverrideIntrinsicSize(
    absl::optional<LayoutUnit>* computed_inline_size,
    absl::optional<LayoutUnit>* computed_block_size) const {
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

  if (ShouldApplyInlineSizeContainment() && !*computed_inline_size)
    *computed_inline_size = LayoutUnit();
  if (ShouldApplyBlockSizeContainment() && !*computed_block_size)
    *computed_block_size = LayoutUnit();
}

}  // namespace blink

#if DCHECK_IS_ON()

CORE_EXPORT void ShowLayoutTree(const blink::NGLayoutInputNode& node) {
  ShowLayoutTree(node.GetLayoutBox());
}

#endif  // DCHECK_IS_ON()
