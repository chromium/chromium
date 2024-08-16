// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_input_node.h"

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_utils.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/inline/inline_node.h"
#include "third_party/blink/renderer/core/layout/intrinsic_sizing_info.h"
#include "third_party/blink/renderer/core/layout/layout_replaced.h"
#include "third_party/blink/renderer/core/layout/layout_result.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/list/layout_list_item.h"
#include "third_party/blink/renderer/core/layout/min_max_sizes.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_column.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_section.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

using mojom::blink::FormControlType;

namespace {

#if DCHECK_IS_ON()
void AppendSubtreeToString(const BlockNode&,
                           const LayoutInputNode* target,
                           StringBuilder*,
                           unsigned indent);

void IndentForDump(const LayoutInputNode& node,
                   const LayoutInputNode* target,
                   StringBuilder* string_builder,
                   unsigned indent) {
  unsigned start_col = 0;
  if (node && target && node == *target) {
    string_builder->Append("*");
    start_col = 1;
  }
  for (unsigned i = start_col; i < indent; i++) {
    string_builder->Append(" ");
  }
}

void AppendNodeToString(const LayoutInputNode& node,
                        const LayoutInputNode* target,
                        StringBuilder* string_builder,
                        unsigned indent = 2) {
  if (!node)
    return;
  DCHECK(string_builder);

  IndentForDump(node, target, string_builder, indent);
  string_builder->Append(node.ToString());
  string_builder->Append("\n");

  if (auto* block_node = DynamicTo<BlockNode>(node)) {
    AppendSubtreeToString(*block_node, target, string_builder, indent + 2);
  } else if (auto* inline_node = DynamicTo<InlineNode>(node)) {
    const auto& items = inline_node->ItemsData(false).items;
    indent += 2;
    for (const InlineItem& inline_item : items) {
      BlockNode child_node(nullptr);
      if (auto* box = DynamicTo<LayoutBox>(inline_item.GetLayoutObject())) {
        child_node = BlockNode(box);
      }
      IndentForDump(child_node, target, string_builder, indent);
      string_builder->Append(inline_item.ToString());
      string_builder->Append("\n");
      if (child_node) {
        // Dump the subtree of an atomic inline, float, block-in-inline, etc.
        AppendSubtreeToString(child_node, target, string_builder, indent + 2);
      }
    }
    DCHECK(!inline_node->NextSibling());
  }
}

void AppendSubtreeToString(const BlockNode& node,
                           const LayoutInputNode* target,
                           StringBuilder* string_builder,
                           unsigned indent) {
  LayoutInputNode first_child = node.FirstChild();
  for (LayoutInputNode node_runner = first_child; node_runner;
       node_runner = node_runner.NextSibling()) {
    AppendNodeToString(node_runner, target, string_builder, indent);
  }
}
#endif

}  // namespace

bool LayoutInputNode::IsSlider() const {
  if (const auto* input = DynamicTo<HTMLInputElement>(box_->GetNode()))
    return input->FormControlType() == FormControlType::kInputRange;
  return false;
}

bool LayoutInputNode::IsSliderThumb() const {
  return IsBlock() && blink::IsSliderThumb(GetDOMNode());
}

bool LayoutInputNode::IsSvgText() const {
  return box_ && box_->IsSVGText();
}

bool LayoutInputNode::IsEmptyTableSection() const {
  return box_->IsTableSection() &&
         To<LayoutTableSection>(box_.Get())->IsEmpty();
}

wtf_size_t LayoutInputNode::TableColumnSpan() const {
  DCHECK(IsTableCol() || IsTableColgroup());
  return To<LayoutTableColumn>(box_.Get())->Span();
}

wtf_size_t LayoutInputNode::TableCellColspan() const {
  DCHECK(box_->IsTableCell());
  return To<LayoutTableCell>(box_.Get())->ColSpan();
}

wtf_size_t LayoutInputNode::TableCellRowspan() const {
  DCHECK(box_->IsTableCell());
  return To<LayoutTableCell>(box_.Get())->ComputedRowSpan();
}

bool LayoutInputNode::IsTextControlPlaceholder() const {
  return IsBlock() && blink::IsTextControlPlaceholder(GetDOMNode());
}

bool LayoutInputNode::IsPaginatedRoot() const {
  if (!IsBlock())
    return false;
  const auto* view = DynamicTo<LayoutView>(box_.Get());
  return view && view->IsFragmentationContextRoot();
}

BlockNode LayoutInputNode::ListMarkerBlockNodeIfListItem() const {
  if (auto* list_item = DynamicTo<LayoutListItem>(box_.Get())) {
    return BlockNode(DynamicTo<LayoutBox>(list_item->Marker()));
  }
  return BlockNode(nullptr);
}

void LayoutInputNode::IntrinsicSize(
    std::optional<LayoutUnit>* computed_inline_size,
    std::optional<LayoutUnit>* computed_block_size) const {
  DCHECK(IsReplaced());

  GetOverrideIntrinsicSize(computed_inline_size, computed_block_size);
  if (*computed_inline_size && *computed_block_size)
    return;

  IntrinsicSizingInfo legacy_sizing_info;
  To<LayoutReplaced>(box_.Get())
      ->ComputeIntrinsicSizingInfo(legacy_sizing_info);

  std::optional<LayoutUnit> intrinsic_inline_size =
      legacy_sizing_info.has_width
          ? std::make_optional(
                LayoutUnit::FromFloatRound(legacy_sizing_info.size.width()))
          : std::nullopt;
  std::optional<LayoutUnit> intrinsic_block_size =
      legacy_sizing_info.has_height
          ? std::make_optional(
                LayoutUnit::FromFloatRound(legacy_sizing_info.size.height()))
          : std::nullopt;
  if (!IsHorizontalWritingMode()) {
    std::swap(intrinsic_inline_size, intrinsic_block_size);
  }

  if (!*computed_inline_size) {
    *computed_inline_size = intrinsic_inline_size;
  }
  if (!*computed_block_size) {
    *computed_block_size = intrinsic_block_size;
  }
}

LayoutInputNode LayoutInputNode::NextSibling() const {
  auto* inline_node = DynamicTo<InlineNode>(this);
  return inline_node ? nullptr : To<BlockNode>(*this).NextSibling();
}

PhysicalSize LayoutInputNode::InitialContainingBlockSize() const {
  gfx::Size icb_size =
      GetDocument().GetLayoutView()->GetLayoutSize(kIncludeScrollbars);
  return PhysicalSize(icb_size);
}

String LayoutInputNode::ToString() const {
  auto* inline_node = DynamicTo<InlineNode>(this);
  return inline_node ? inline_node->ToString()
                     : To<BlockNode>(*this).ToString();
}

#if DCHECK_IS_ON()
String LayoutInputNode::DumpNodeTree(const LayoutInputNode* target) const {
  StringBuilder string_builder;
  string_builder.Append(".:: Layout input node tree ::.\n");
  AppendNodeToString(*this, target, &string_builder);
  return string_builder.ToString();
}

String LayoutInputNode::DumpNodeTreeFromRoot() const {
  return BlockNode(box_->View()).DumpNodeTree(this);
}

void LayoutInputNode::ShowNodeTree(const LayoutInputNode* target) const {
  if (getenv("RUNNING_UNDER_RR")) {
    // Printing timestamps requires an IPC to get the local time, which
    // does not work in an rr replay session. Just disable timestamp printing
    // globally, since we don't need them. Affecting global state isn't a
    // problem because invoking this from a rr session creates a temporary
    // program environment that will be destroyed as soon as the invocation
    // completes.
    logging::SetLogItems(true, true, false, false);
  }

  DLOG(INFO) << "\n" << DumpNodeTree(target).Utf8();
}

void LayoutInputNode::ShowNodeTreeFromRoot() const {
  BlockNode(box_->View()).ShowNodeTree(this);
}
#endif

void LayoutInputNode::GetOverrideIntrinsicSize(
    std::optional<LayoutUnit>* computed_inline_size,
    std::optional<LayoutUnit>* computed_block_size) const {
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

CORE_EXPORT void ShowLayoutTree(const blink::LayoutInputNode& node) {
  ShowLayoutTree(node.GetLayoutBox());
}

#endif  // DCHECK_IS_ON()
