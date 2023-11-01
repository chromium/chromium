// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_layout_input_node.h"

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_utils.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/inline/inline_node.h"
#include "third_party/blink/renderer/core/layout/intrinsic_sizing_info.h"
#include "third_party/blink/renderer/core/layout/layout_replaced.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/list/layout_list_item.h"
#include "third_party/blink/renderer/core/layout/min_max_sizes.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_column.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_section.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

using mojom::blink::FormControlType;

namespace {

#if DCHECK_IS_ON()
void AppendSubtreeToString(const NGBlockNode&,
                           const NGLayoutInputNode* target,
                           StringBuilder*,
                           unsigned indent);

void IndentForDump(const NGLayoutInputNode& node,
                   const NGLayoutInputNode* target,
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

void AppendNodeToString(const NGLayoutInputNode& node,
                        const NGLayoutInputNode* target,
                        StringBuilder* string_builder,
                        unsigned indent = 2) {
  if (!node)
    return;
  DCHECK(string_builder);

  IndentForDump(node, target, string_builder, indent);
  string_builder->Append(node.ToString());
  string_builder->Append("\n");

  if (auto* block_node = DynamicTo<NGBlockNode>(node)) {
    AppendSubtreeToString(*block_node, target, string_builder, indent + 2);
  } else if (auto* inline_node = DynamicTo<InlineNode>(node)) {
    const auto& items = inline_node->ItemsData(false).items;
    indent += 2;
    for (const InlineItem& inline_item : items) {
      NGBlockNode child_node(nullptr);
      if (auto* box = DynamicTo<LayoutBox>(inline_item.GetLayoutObject())) {
        child_node = NGBlockNode(box);
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

void AppendSubtreeToString(const NGBlockNode& node,
                           const NGLayoutInputNode* target,
                           StringBuilder* string_builder,
                           unsigned indent) {
  NGLayoutInputNode first_child = node.FirstChild();
  for (NGLayoutInputNode node_runner = first_child; node_runner;
       node_runner = node_runner.NextSibling()) {
    AppendNodeToString(node_runner, target, string_builder, indent);
  }
}
#endif

}  // namespace

bool NGLayoutInputNode::IsSlider() const {
  if (const auto* input = DynamicTo<HTMLInputElement>(box_->GetNode()))
    return input->FormControlType() == FormControlType::kInputRange;
  return false;
}

bool NGLayoutInputNode::IsSliderThumb() const {
  return IsBlock() && blink::IsSliderThumb(GetDOMNode());
}

bool NGLayoutInputNode::IsSvgText() const {
  return box_ && box_->IsSVGText();
}

bool NGLayoutInputNode::IsEmptyTableSection() const {
  return box_->IsTableSection() &&
         To<LayoutTableSection>(box_.Get())->IsEmpty();
}

wtf_size_t NGLayoutInputNode::TableColumnSpan() const {
  DCHECK(IsTableCol() || IsTableColgroup());
  return To<LayoutTableColumn>(box_.Get())->Span();
}

wtf_size_t NGLayoutInputNode::TableCellColspan() const {
  DCHECK(box_->IsTableCell());
  return To<LayoutTableCell>(box_.Get())->ColSpan();
}

wtf_size_t NGLayoutInputNode::TableCellRowspan() const {
  DCHECK(box_->IsTableCell());
  return To<LayoutTableCell>(box_.Get())->ComputedRowSpan();
}

bool NGLayoutInputNode::IsTextControlPlaceholder() const {
  return IsBlock() && blink::IsTextControlPlaceholder(GetDOMNode());
}

bool NGLayoutInputNode::IsPaginatedRoot() const {
  if (!IsBlock())
    return false;
  const auto* view = DynamicTo<LayoutView>(box_.Get());
  return view && view->IsFragmentationContextRoot();
}

NGBlockNode NGLayoutInputNode::ListMarkerBlockNodeIfListItem() const {
  if (auto* list_item = DynamicTo<LayoutListItem>(box_.Get())) {
    return NGBlockNode(DynamicTo<LayoutBox>(list_item->Marker()));
  }
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

  absl::optional<LayoutUnit> intrinsic_inline_size =
      legacy_sizing_info.has_width
          ? absl::make_optional(
                LayoutUnit::FromFloatRound(legacy_sizing_info.size.width()))
          : absl::nullopt;
  absl::optional<LayoutUnit> intrinsic_block_size =
      legacy_sizing_info.has_height
          ? absl::make_optional(
                LayoutUnit::FromFloatRound(legacy_sizing_info.size.height()))
          : absl::nullopt;
  if (!IsHorizontalWritingMode(Style().GetWritingMode())) {
    std::swap(intrinsic_inline_size, intrinsic_block_size);
  }

  if (!*computed_inline_size) {
    *computed_inline_size = intrinsic_inline_size;
  }
  if (!*computed_block_size) {
    *computed_block_size = intrinsic_block_size;
  }
}

NGLayoutInputNode NGLayoutInputNode::NextSibling() const {
  auto* inline_node = DynamicTo<InlineNode>(this);
  return inline_node ? nullptr : To<NGBlockNode>(*this).NextSibling();
}

PhysicalSize NGLayoutInputNode::InitialContainingBlockSize() const {
  gfx::Size icb_size =
      GetDocument().GetLayoutView()->GetLayoutSize(kIncludeScrollbars);
  return PhysicalSize(icb_size);
}

String NGLayoutInputNode::ToString() const {
  auto* inline_node = DynamicTo<InlineNode>(this);
  return inline_node ? inline_node->ToString()
                     : To<NGBlockNode>(*this).ToString();
}

#if DCHECK_IS_ON()
String NGLayoutInputNode::DumpNodeTree(const NGLayoutInputNode* target) const {
  StringBuilder string_builder;
  string_builder.Append(".:: Layout input node tree ::.\n");
  AppendNodeToString(*this, target, &string_builder);
  return string_builder.ToString();
}

String NGLayoutInputNode::DumpNodeTreeFromRoot() const {
  return NGBlockNode(box_->View()).DumpNodeTree(this);
}

void NGLayoutInputNode::ShowNodeTree(const NGLayoutInputNode* target) const {
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

void NGLayoutInputNode::ShowNodeTreeFromRoot() const {
  NGBlockNode(box_->View()).ShowNodeTree(this);
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
