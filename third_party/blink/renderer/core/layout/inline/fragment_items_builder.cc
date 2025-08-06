// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/fragment_items_builder.h"

#include "third_party/blink/renderer/core/layout/box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/inline/fragment_items.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/svg/svg_text_layout_algorithm.h"

namespace blink {

FragmentItemsBuilder::FragmentItemsBuilder(
    WritingDirectionMode writing_direction)
    : node_(nullptr), writing_direction_(writing_direction) {}

FragmentItemsBuilder::FragmentItemsBuilder(
    const InlineNode& node,
    WritingDirectionMode writing_direction,
    bool is_block_fragmented)
    : node_(node), writing_direction_(writing_direction) {
  const InlineItemsData& items_data = node.ItemsData(false);
  text_content_ = items_data.text_content;
  const InlineItemsData& first_line = node.ItemsData(true);
  if (&items_data != &first_line)
    first_line_text_content_ = first_line.text_content;

  // For a very large inline formatting context, the vector reallocation becomes
  // hot. Estimate the number of items by assuming 40 characters can fit in a
  // line, and each line contains 3 items; a line box, an inline box, and a
  // text. If it will require more than one reallocations, make an initial
  // reservation here.
  //
  // Skip this if we constrained by a fragmentainer's block-size. The estimate
  // will be way too high in such cases, and we're going to make this
  // reservation for every fragmentainer, potentially running out of memory if
  // oilpan doesn't get around to collecting it.
  if (!is_block_fragmented) {
    const wtf_size_t estimated_item_count = text_content_.length() / 40 * 3;
    if (estimated_item_count > items_.capacity() * 2) [[unlikely]] {
      items_.ReserveInitialCapacity(estimated_item_count);
    }
  }
}

FragmentItemsBuilder::~FragmentItemsBuilder() {
  ReleaseCurrentLogicalLineContainer();

  // Delete leftovers that were associated, but were not added. Clear() is
  // explicitly called here for memory performance.
  DCHECK(line_container_pool_);
  line_container_pool_->Clear();
  for (const auto& i : line_container_map_) {
    if (i.value != line_container_pool_) {
      i.value->Clear();
    }
  }
}

void FragmentItemsBuilder::ReleaseCurrentLogicalLineContainer() {
  if (!current_line_container_) {
    return;
  }
  if (current_line_container_ == line_container_pool_) {
    DCHECK(is_line_items_pool_acquired_);
    is_line_items_pool_acquired_ = false;
  } else {
    current_line_container_->Clear();
  }
  current_line_container_ = nullptr;
}

void FragmentItemsBuilder::MoveCurrentLogicalLineItemsToMap() {
  if (!current_line_container_) {
    DCHECK(!current_line_fragment_);
    return;
  }
  DCHECK(current_line_fragment_);
  line_container_map_.insert(current_line_fragment_, current_line_container_);
  current_line_fragment_ = nullptr;
  current_line_container_ = nullptr;
}

LogicalLineContainer* FragmentItemsBuilder::AcquireLogicalLineContainer() {
  if (line_container_pool_ && !is_line_items_pool_acquired_) {
    is_line_items_pool_acquired_ = true;
    return line_container_pool_;
  }
  MoveCurrentLogicalLineItemsToMap();
  DCHECK(!current_line_container_);
  current_line_container_ = MakeGarbageCollected<LogicalLineContainer>();
  return current_line_container_;
}

const LogicalLineItems& FragmentItemsBuilder::GetLogicalLineItems(
    const PhysicalLineBoxFragment& line_fragment) const {
  if (&line_fragment == current_line_fragment_) {
    DCHECK(current_line_container_);
    return current_line_container_->BaseLine();
  }
  const LogicalLineContainer* container =
      line_container_map_.at(&line_fragment);
  DCHECK(container);
  return container->BaseLine();
}

void FragmentItemsBuilder::AssociateLogicalLineContainer(
    LogicalLineContainer* line_container,
    const PhysicalFragment& line_fragment) {
  DCHECK(!current_line_container_ || current_line_container_ == line_container);
  current_line_container_ = line_container;
  DCHECK(!current_line_fragment_);
  current_line_fragment_ = &line_fragment;
}

void FragmentItemsBuilder::AddLine(const PhysicalLineBoxFragment& line_fragment,
                                   const LogicalOffset& offset) {
  DCHECK(!is_converted_to_physical_);
  if (&line_fragment == current_line_fragment_) {
    DCHECK(current_line_container_);
    current_line_fragment_ = nullptr;
  } else {
    MoveCurrentLogicalLineItemsToMap();
    DCHECK(!current_line_container_);
    current_line_container_ = line_container_map_.Take(&line_fragment);
    DCHECK(current_line_container_);
  }
  LogicalLineContainer* line_container = current_line_container_;
  LogicalLineItems& line_items = line_container->BaseLine();

  // Reserve the capacity for (children + line box item).
  const wtf_size_t size_before = items_.size();
  const wtf_size_t estimated_size =
      size_before + line_container->EstimatedFragmentItemCount();
  const wtf_size_t old_capacity = items_.capacity();
  if (estimated_size > old_capacity)
    items_.reserve(std::max(estimated_size, old_capacity * 2));

  // Add an empty item so that the start of the line can be set later.
  const wtf_size_t line_start_index = items_.size();
  items_.emplace_back(offset, line_fragment);

  AddItems(base::span(line_items));

  for (auto& annotation_line : line_container->AnnotationLineList()) {
    const wtf_size_t annotation_line_start_index = items_.size();
    const LayoutUnit line_height = annotation_line.metrics.LineHeight();
    if (!annotation_line->FirstInFlowChild()) {
      continue;
    }

    // If the line is hidden (e.g. because of line-clamp), annotations on that
    // line should be hidden as well.
    if (line_fragment.IsHiddenForPaint()) {
      for (auto& item : *annotation_line.line_items) {
        item.is_hidden_for_paint = true;
      }
    }

    LogicalOffset line_offset = annotation_line->FirstInFlowChild()->Offset();
    LayoutUnit line_inline_size =
        annotation_line->LastInFlowChild()->rect.InlineEndOffset() -
        line_offset.inline_offset;
    PhysicalSize size = IsHorizontalWritingMode(GetWritingMode())
                            ? PhysicalSize(line_inline_size, line_height)
                            : PhysicalSize(line_height, line_inline_size);
    // The offset must be relative to the base line box for now.
    items_.emplace_back(line_offset, size, line_fragment);
    AddItems(base::span(*annotation_line.line_items));
    items_[annotation_line_start_index].item.SetDescendantsCount(
        items_.size() - annotation_line_start_index);
  }

  // All children are added. Create an item for the start of the line.
  FragmentItem& line_item = items_[line_start_index].item;
  const wtf_size_t item_count = items_.size() - line_start_index;
  DCHECK_EQ(line_item.DescendantsCount(), 1u);
  line_item.SetDescendantsCount(item_count);

  // Keep children's offsets relative to |line|. They will be adjusted later in
  // |ConvertToPhysical()|.

  ReleaseCurrentLogicalLineContainer();

  DCHECK_LE(items_.size(), estimated_size);
}

void FragmentItemsBuilder::AddItems(base::span<LogicalLineItem> child_span) {
  DCHECK(!is_converted_to_physical_);

  const WritingMode writing_mode = GetWritingMode();
  for (size_t i = 0; i < child_span.size();) {
    LogicalLineItem& child = child_span[i];
    // OOF children should have been added to their parent box fragments.
    DCHECK(!child.out_of_flow_positioned_box);
    if (!child.CanCreateFragmentItem()) {
      ++i;
      continue;
    }

    if (child.children_count <= 1) {
      items_.emplace_back(child.rect.offset, std::move(child), writing_mode);
      ++i;
      continue;
    }

    const unsigned children_count = child.children_count;
    // Children of inline boxes are flattened and added to |items_|, with the
    // count of descendant items to preserve the tree structure.
    //
    // Add an empty item so that the start of the box can be set later.
    const wtf_size_t box_start_index = items_.size();
    items_.emplace_back(child.rect.offset, std::move(child), writing_mode);

    // Add all children, including their desendants, skipping this item.
    CHECK_GE(children_count, 1u);  // 0 will loop infinitely.
    AddItems(child_span.subspan(i + 1, children_count - 1));
    i += children_count;

    // All children are added. Compute how many items are actually added. The
    // number of items added may be different from |children_count|.
    const wtf_size_t item_count = items_.size() - box_start_index;
    FragmentItem& box_item = items_[box_start_index].item;
    DCHECK_EQ(box_item.DescendantsCount(), 1u);
    box_item.SetDescendantsCount(item_count);
  }
}

void FragmentItemsBuilder::AddListMarker(
    const PhysicalBoxFragment& marker_fragment,
    const LogicalOffset& offset) {
  DCHECK(!is_converted_to_physical_);

  // Resolved direction matters only for inline items, and outside list markers
  // are not inline.
  const TextDirection resolved_direction = TextDirection::kLtr;
  items_.emplace_back(offset, marker_fragment, resolved_direction);
}

FragmentItemsBuilder::AddPreviousItemsResult
FragmentItemsBuilder::AddPreviousItems(const PhysicalBoxFragment& container,
                                       const FragmentItems& items,
                                       const FragmentItem& end_item,
                                       BoxFragmentBuilder* container_builder,
                                       wtf_size_t max_lines) {
  DCHECK(node_);
  DCHECK(container_builder);
  DCHECK(text_content_);

  if (items.FirstLineText() && !first_line_text_content_) [[unlikely]] {
    // Don't reuse previous items if they have different `::first-line` style
    // but |this| doesn't. Reaching here means that computed style doesn't
    // change, but |FragmentItem| has wrong |StyleVariant|.
    return AddPreviousItemsResult();
  }

  DCHECK(items_.empty());
  const FragmentItems::Span source_items = items.Items();
  const wtf_size_t estimated_size =
      base::checked_cast<wtf_size_t>(source_items.size());
  items_.reserve(estimated_size);

  // Convert offsets to logical. The logic is opposite to |ConvertToPhysical|.
  // This is needed because the container size may be different, in that case,
  // the physical offsets are different when `writing-mode: vertial-rl`.
  DCHECK(!is_converted_to_physical_);
  const WritingModeConverter converter(GetWritingDirection(), container.Size());
  const WritingMode writing_mode = GetWritingMode();
  WritingModeConverter line_converter(
      {ToLineWritingMode(writing_mode), TextDirection::kLtr});

  const InlineBreakToken* last_break_token = nullptr;
  const InlineItemsData* items_data = nullptr;
  LayoutUnit used_block_size;
  wtf_size_t line_count = 0;

  for (InlineCursor cursor(container, items); cursor;
       cursor.MoveToNextSkippingChildren()) {
    DCHECK(cursor.Current().Item());
    const FragmentItem& item = *cursor.Current().Item();
    if (&item == &end_item) {
      break;
    }
    DCHECK(!item.IsDirty());

    const LogicalOffset item_offset =
        converter.ToLogical(item.OffsetInContainerFragment(), item.Size());

    DCHECK_EQ(item.Type(), FragmentItem::kLine);
    DCHECK(item.LineBoxFragment());

    // Check if this line has valid item_index and offset.
    const PhysicalLineBoxFragment* line_fragment = item.LineBoxFragment();
    // Block-in-inline should have been prevented by |EndOfReusableItems|.
    DCHECK(!line_fragment->IsBlockInInline());
    const auto* break_token =
        To<InlineBreakToken>(line_fragment->GetBreakToken());
    DCHECK(break_token);
    const InlineItemsData* current_items_data;
    if (break_token->UseFirstLineStyle()) [[unlikely]] {
      current_items_data = &node_.ItemsData(true);
    } else if (items_data) {
      current_items_data = items_data;
    } else {
      current_items_data = items_data = &node_.ItemsData(false);
    }
    if (!current_items_data->IsValidOffset(break_token->Start())) [[unlikely]] {
      DUMP_WILL_BE_NOTREACHED();
      break;
    }

    last_break_token = break_token;
    container_builder->AddChild(*line_fragment, item_offset);
    used_block_size += ToLogicalSize(item.Size(), writing_mode).block_size;

    items_.emplace_back(item_offset, item);
    const PhysicalRect line_box_bounds = item.RectInContainerFragment();
    line_converter.SetOuterSize(line_box_bounds.size);
    for (InlineCursor line = cursor.CursorForDescendants(); line;
         line.MoveToNext()) {
      const FragmentItem& line_child = *line.Current().Item();
      if (line_child.Type() != FragmentItem::kLine) {
        // The caller has computed the range safe to reuse by calling
        // |EndOfReusableItems|. All children should be safe to reuse.
        DCHECK(line_child.CanReuse());
      }
#if DCHECK_IS_ON()
      // |RebuildFragmentTreeSpine| does not rebuild spine if |NeedsLayout|.
      // Such block needs to copy PostLayout fragment while running simplified
      // layout.
      std::optional<PhysicalBoxFragment::AllowPostLayoutScope>
          allow_post_layout;
      if (line_child.IsRelayoutBoundary()) {
        allow_post_layout.emplace();
      }
#endif
      items_.emplace_back(
          line_converter.ToLogical(
              line_child.OffsetInContainerFragment() - line_box_bounds.offset,
              line_child.Size()),
          line_child);

      // Be sure to pick the post-layout fragment.
      const FragmentItem& new_item = items_.back().item;
      if (const PhysicalBoxFragment* box = new_item.BoxFragment()) {
        box = box->PostLayout();
        new_item.GetMutableForCloning().ReplaceBoxFragment(*box);
      }
    }
    if (++line_count == max_lines) {
      break;
    }
  }
  DCHECK_LE(items_.size(), estimated_size);

  if (last_break_token) {
    DCHECK_GT(line_count, 0u);
    DCHECK(!max_lines || line_count <= max_lines);
    return AddPreviousItemsResult{last_break_token, used_block_size, line_count,
                                  true};
  }
  return AddPreviousItemsResult();
}

const FragmentItemsBuilder::ItemWithOffsetList& FragmentItemsBuilder::Items(
    const PhysicalSize& outer_size) {
  ConvertToPhysical(outer_size);
  return items_;
}

// Convert internal logical offsets to physical. Items are kept with logical
// offset until outer box size is determined.
void FragmentItemsBuilder::ConvertToPhysical(const PhysicalSize& outer_size) {
  if (is_converted_to_physical_)
    return;

  const WritingModeConverter converter(GetWritingDirection(), outer_size);
  // Children of lines have line-relative offsets. Use line-writing mode to
  // convert their logical offsets. Use `kLtr` because inline items are after
  // bidi-reoder, and that their offset is visual, not logical.
  WritingModeConverter line_converter(
      {ToLineWritingMode(GetWritingMode()), TextDirection::kLtr});

  for (wtf_size_t i = 0; i < items_.size(); ++i) {
    ItemWithOffset& item_with_offset = items_[i];
    FragmentItem* item = &item_with_offset.item;
    item->SetOffset(
        converter.ToPhysical(item_with_offset.offset, item->Size()));

    // Transform children of lines separately from children of the block,
    // because they may have different directions from the block. To do
    // this, their offsets are relative to their containing line box.
    if (item->Type() == FragmentItem::kLine) {
      unsigned descendants_count = item->DescendantsCount();
      DCHECK(descendants_count);
      if (descendants_count) {
        const PhysicalRect line_box_bounds = item->RectInContainerFragment();
        line_converter.SetOuterSize(line_box_bounds.size);
        while (--descendants_count) {
          ++i;
          CHECK_NE(i, items_.size());
          ItemWithOffset& descendant_item_with_offset = items_[i];
          item = &descendant_item_with_offset.item;
          item->SetOffset(
              line_converter.ToPhysical(descendant_item_with_offset.offset,
                                        item->Size()) +
              line_box_bounds.offset);
        }
      }
    }
  }

  is_converted_to_physical_ = true;
}

void FragmentItemsBuilder::MoveChildrenInDirection(LayoutUnit offset,
                                                   bool is_block_direction) {
  DCHECK(!is_converted_to_physical_);
  for (wtf_size_t i = 0; i < items_.size(); ++i) {
    ItemWithOffset& item_with_offset = items_[i];
    FragmentItem* item = &item_with_offset.item;
    if (item->Type() == FragmentItem::kLine) {
      if (is_block_direction) {
        item_with_offset.offset.block_offset += offset;
      } else {
        item_with_offset.offset.inline_offset += offset;
      }
      i += item->DescendantsCount() - 1;
      DCHECK_LE(i, items_.size());
      continue;
    }
    if (is_block_direction) {
      item_with_offset.offset.block_offset += offset;
    } else {
      item_with_offset.offset.inline_offset += offset;
    }
  }
}

std::optional<PhysicalSize> FragmentItemsBuilder::ToFragmentItems(
    const PhysicalSize& outer_size,
    void* data) {
  DCHECK(text_content_);
  ConvertToPhysical(outer_size);
  std::optional<PhysicalSize> new_size;
  if (node_.IsSvgText()) {
    new_size = SvgTextLayoutAlgorithm(node_, GetWritingMode())
                   .Layout(TextContent(false), items_);
  }
  new (data) FragmentItems(this);
  return new_size;
}

}  // namespace blink
