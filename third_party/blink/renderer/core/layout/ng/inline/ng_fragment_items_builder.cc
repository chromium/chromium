// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items_builder.h"

#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/svg/svg_text_layout_algorithm.h"

namespace blink {

NGFragmentItemsBuilder::NGFragmentItemsBuilder(
    WritingDirectionMode writing_direction)
    : node_(nullptr), writing_direction_(writing_direction) {}

NGFragmentItemsBuilder::NGFragmentItemsBuilder(
    const NGInlineNode& node,
    WritingDirectionMode writing_direction,
    bool is_block_fragmented)
    : node_(node), writing_direction_(writing_direction) {
  const NGInlineItemsData& items_data = node.ItemsData(false);
  text_content_ = items_data.text_content;
  const NGInlineItemsData& first_line = node.ItemsData(true);
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
    if (UNLIKELY(estimated_item_count > items_.capacity() * 2)) {
      items_.ReserveInitialCapacity(estimated_item_count);
    }
  }
}

NGFragmentItemsBuilder::~NGFragmentItemsBuilder() {
  ReleaseCurrentLogicalLineItems();

  // Delete leftovers that were associated, but were not added. Clear() is
  // explicitly called here for memory performance.
  DCHECK(line_items_pool_);
  line_items_pool_->clear();
  for (const auto& i : line_items_map_) {
    if (i.value != line_items_pool_)
      i.value->clear();
  }
}

void NGFragmentItemsBuilder::ReleaseCurrentLogicalLineItems() {
  if (!current_line_items_)
    return;
  if (current_line_items_ == line_items_pool_) {
    DCHECK(is_line_items_pool_acquired_);
    is_line_items_pool_acquired_ = false;
  } else {
    current_line_items_->clear();
  }
  current_line_items_ = nullptr;
}

void NGFragmentItemsBuilder::MoveCurrentLogicalLineItemsToMap() {
  if (!current_line_items_) {
    DCHECK(!current_line_fragment_);
    return;
  }
  DCHECK(current_line_fragment_);
  line_items_map_.insert(current_line_fragment_, current_line_items_);
  current_line_fragment_ = nullptr;
  current_line_items_ = nullptr;
}

NGLogicalLineItems* NGFragmentItemsBuilder::AcquireLogicalLineItems() {
  if (line_items_pool_ && !is_line_items_pool_acquired_) {
    is_line_items_pool_acquired_ = true;
    return line_items_pool_;
  }
  MoveCurrentLogicalLineItemsToMap();
  DCHECK(!current_line_items_);
  current_line_items_ = MakeGarbageCollected<NGLogicalLineItems>();
  return current_line_items_;
}

const NGLogicalLineItems& NGFragmentItemsBuilder::LogicalLineItems(
    const NGPhysicalLineBoxFragment& line_fragment) const {
  if (&line_fragment == current_line_fragment_) {
    DCHECK(current_line_items_);
    return *current_line_items_;
  }
  const NGLogicalLineItems* items = line_items_map_.at(&line_fragment);
  DCHECK(items);
  return *items;
}

void NGFragmentItemsBuilder::AssociateLogicalLineItems(
    NGLogicalLineItems* line_items,
    const NGPhysicalFragment& line_fragment) {
  DCHECK(!current_line_items_ || current_line_items_ == line_items);
  current_line_items_ = line_items;
  DCHECK(!current_line_fragment_);
  current_line_fragment_ = &line_fragment;
}

void NGFragmentItemsBuilder::AddLine(
    const NGPhysicalLineBoxFragment& line_fragment,
    const LogicalOffset& offset) {
  DCHECK(!is_converted_to_physical_);
  if (&line_fragment == current_line_fragment_) {
    DCHECK(current_line_items_);
    current_line_fragment_ = nullptr;
  } else {
    MoveCurrentLogicalLineItemsToMap();
    DCHECK(!current_line_items_);
    current_line_items_ = line_items_map_.Take(&line_fragment);
    DCHECK(current_line_items_);
  }
  NGLogicalLineItems* line_items = current_line_items_;

  // Reserve the capacity for (children + line box item).
  const wtf_size_t size_before = items_.size();
  const wtf_size_t estimated_size = size_before + line_items->size() + 1;
  const wtf_size_t old_capacity = items_.capacity();
  if (estimated_size > old_capacity)
    items_.reserve(std::max(estimated_size, old_capacity * 2));

  // Add an empty item so that the start of the line can be set later.
  const wtf_size_t line_start_index = items_.size();
  items_.emplace_back(offset, line_fragment);

  AddItems(line_items->begin(), line_items->end());

  // All children are added. Create an item for the start of the line.
  NGFragmentItem& line_item = items_[line_start_index].item;
  const wtf_size_t item_count = items_.size() - line_start_index;
  DCHECK_EQ(line_item.DescendantsCount(), 1u);
  line_item.SetDescendantsCount(item_count);

  // Keep children's offsets relative to |line|. They will be adjusted later in
  // |ConvertToPhysical()|.

  ReleaseCurrentLogicalLineItems();

  DCHECK_LE(items_.size(), estimated_size);
}

void NGFragmentItemsBuilder::AddItems(NGLogicalLineItem* child_begin,
                                      NGLogicalLineItem* child_end) {
  DCHECK(!is_converted_to_physical_);

  const WritingMode writing_mode = GetWritingMode();
  for (NGLogicalLineItem* child_iter = child_begin; child_iter != child_end;) {
    NGLogicalLineItem& child = *child_iter;
    // OOF children should have been added to their parent box fragments.
    DCHECK(!child.out_of_flow_positioned_box);
    if (!child.CanCreateFragmentItem()) {
      ++child_iter;
      continue;
    }

    if (child.children_count <= 1) {
      items_.emplace_back(child.rect.offset, std::move(child), writing_mode);
      ++child_iter;
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
    NGLogicalLineItem* end_child_iter = child_iter + children_count;
    CHECK_LE(end_child_iter - child_begin, child_end - child_begin);
    AddItems(child_iter + 1, end_child_iter);
    child_iter = end_child_iter;

    // All children are added. Compute how many items are actually added. The
    // number of items added may be different from |children_count|.
    const wtf_size_t item_count = items_.size() - box_start_index;
    NGFragmentItem& box_item = items_[box_start_index].item;
    DCHECK_EQ(box_item.DescendantsCount(), 1u);
    box_item.SetDescendantsCount(item_count);
  }
}

void NGFragmentItemsBuilder::AddListMarker(
    const NGPhysicalBoxFragment& marker_fragment,
    const LogicalOffset& offset) {
  DCHECK(!is_converted_to_physical_);

  // Resolved direction matters only for inline items, and outside list markers
  // are not inline.
  const TextDirection resolved_direction = TextDirection::kLtr;
  items_.emplace_back(offset, marker_fragment, resolved_direction);
}

NGFragmentItemsBuilder::AddPreviousItemsResult
NGFragmentItemsBuilder::AddPreviousItems(
    const NGPhysicalBoxFragment& container,
    const NGFragmentItems& items,
    NGBoxFragmentBuilder* container_builder,
    const NGFragmentItem* end_item,
    wtf_size_t max_lines) {
  if (end_item) {
    DCHECK(node_);
    DCHECK(container_builder);
    DCHECK(text_content_);

    if (UNLIKELY(items.FirstLineText() && !first_line_text_content_)) {
      // Don't reuse previous items if they have different `::first-line` style
      // but |this| doesn't. Reaching here means that computed style doesn't
      // change, but |NGFragmentItem| has wrong |NGStyleVariant|.
      return AddPreviousItemsResult();
    }
  } else {
    DCHECK(!container_builder);
    DCHECK(!text_content_);
    text_content_ = items.NormalText();
    first_line_text_content_ = items.FirstLineText();
  }

  DCHECK(items_.empty());
  const NGFragmentItems::Span source_items = items.Items();
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

  const NGInlineBreakToken* last_break_token = nullptr;
  const NGInlineItemsData* items_data = nullptr;
  LayoutUnit used_block_size;
  wtf_size_t line_count = 0;

  for (NGInlineCursor cursor(container, items); cursor;) {
    DCHECK(cursor.Current().Item());
    const NGFragmentItem& item = *cursor.Current().Item();
    if (&item == end_item)
      break;
    DCHECK(!item.IsDirty());

    const LogicalOffset item_offset =
        converter.ToLogical(item.OffsetInContainerFragment(), item.Size());

    if (item.Type() == NGFragmentItem::kLine) {
      DCHECK(item.LineBoxFragment());
      if (end_item) {
        // Check if this line has valid item_index and offset.
        const NGPhysicalLineBoxFragment* line_fragment = item.LineBoxFragment();
        // Block-in-inline should have been prevented by |EndOfReusableItems|.
        DCHECK(!line_fragment->IsBlockInInline());
        const NGInlineBreakToken* break_token =
            To<NGInlineBreakToken>(line_fragment->BreakToken());
        DCHECK(break_token);
        const NGInlineItemsData* current_items_data;
        if (UNLIKELY(break_token->UseFirstLineStyle()))
          current_items_data = &node_.ItemsData(true);
        else if (items_data)
          current_items_data = items_data;
        else
          current_items_data = items_data = &node_.ItemsData(false);
        if (UNLIKELY(
                !current_items_data->IsValidOffset(break_token->Start()))) {
          NOTREACHED();
          break;
        }

        last_break_token = break_token;
        container_builder->AddChild(*line_fragment, item_offset);
        used_block_size +=
            item.Size().ConvertToLogical(writing_mode).block_size;
      }

      items_.emplace_back(item_offset, item);
      const PhysicalRect line_box_bounds = item.RectInContainerFragment();
      line_converter.SetOuterSize(line_box_bounds.size);
      for (NGInlineCursor line = cursor.CursorForDescendants(); line;
           line.MoveToNext()) {
        const NGFragmentItem& line_child = *line.Current().Item();
        if (end_item) {
          // If |end_item| is given, the caller has computed the range safe to
          // reuse by calling |EndOfReusableItems|. All children should be safe
          // to reuse.
          DCHECK(line_child.CanReuse());
        } else if (!line_child.CanReuse()) {
          // Abort and report the failure if any child is not reusable.
          return AddPreviousItemsResult();
        }
#if DCHECK_IS_ON()
        // |RebuildFragmentTreeSpine| does not rebuild spine if |NeedsLayout|.
        // Such block needs to copy PostLayout fragment while running simplified
        // layout.
        absl::optional<NGPhysicalBoxFragment::AllowPostLayoutScope>
            allow_post_layout;
        if (line_child.IsRelayoutBoundary())
          allow_post_layout.emplace();
#endif
        items_.emplace_back(
            line_converter.ToLogical(
                line_child.OffsetInContainerFragment() - line_box_bounds.offset,
                line_child.Size()),
            line_child);

        // Be sure to pick the post-layout fragment.
        const NGFragmentItem& new_item = items_.back().item;
        if (const NGPhysicalBoxFragment* box = new_item.BoxFragment()) {
          box = box->PostLayout();
          new_item.GetMutableForCloning().ReplaceBoxFragment(*box);
        }
      }
      if (++line_count == max_lines)
        break;
      cursor.MoveToNextSkippingChildren();
      continue;
    }

    DCHECK_NE(item.Type(), NGFragmentItem::kLine);
    DCHECK(!end_item);
    items_.emplace_back(item_offset, item);
    cursor.MoveToNext();
  }
  DCHECK_LE(items_.size(), estimated_size);

  if (end_item && last_break_token) {
    DCHECK_GT(line_count, 0u);
    DCHECK(!max_lines || line_count <= max_lines);
    return AddPreviousItemsResult{last_break_token, used_block_size, line_count,
                                  true};
  }
  return AddPreviousItemsResult();
}

const NGFragmentItemsBuilder::ItemWithOffsetList& NGFragmentItemsBuilder::Items(
    const PhysicalSize& outer_size) {
  ConvertToPhysical(outer_size);
  return items_;
}

// Convert internal logical offsets to physical. Items are kept with logical
// offset until outer box size is determined.
void NGFragmentItemsBuilder::ConvertToPhysical(const PhysicalSize& outer_size) {
  if (is_converted_to_physical_)
    return;

  const WritingModeConverter converter(GetWritingDirection(), outer_size);
  // Children of lines have line-relative offsets. Use line-writing mode to
  // convert their logical offsets. Use `kLtr` because inline items are after
  // bidi-reoder, and that their offset is visual, not logical.
  WritingModeConverter line_converter(
      {ToLineWritingMode(GetWritingMode()), TextDirection::kLtr});

  for (ItemWithOffset* iter = items_.begin(); iter != items_.end(); ++iter) {
    NGFragmentItem* item = &iter->item;
    item->SetOffset(converter.ToPhysical(iter->offset, item->Size()));

    // Transform children of lines separately from children of the block,
    // because they may have different directions from the block. To do
    // this, their offsets are relative to their containing line box.
    if (item->Type() == NGFragmentItem::kLine) {
      unsigned descendants_count = item->DescendantsCount();
      DCHECK(descendants_count);
      if (descendants_count) {
        const PhysicalRect line_box_bounds = item->RectInContainerFragment();
        line_converter.SetOuterSize(line_box_bounds.size);
        while (--descendants_count) {
          ++iter;
          DCHECK_NE(iter, items_.end());
          item = &iter->item;
          item->SetOffset(
              line_converter.ToPhysical(iter->offset, item->Size()) +
              line_box_bounds.offset);
        }
      }
    }
  }

  is_converted_to_physical_ = true;
}

absl::optional<LogicalOffset> NGFragmentItemsBuilder::LogicalOffsetFor(
    const LayoutObject& layout_object) const {
  for (const ItemWithOffset& item : items_) {
    if (item->GetLayoutObject() == &layout_object)
      return item.offset;
  }
  return absl::nullopt;
}

void NGFragmentItemsBuilder::MoveChildrenInBlockDirection(LayoutUnit delta) {
  DCHECK(!is_converted_to_physical_);
  for (ItemWithOffset* iter = items_.begin(); iter != items_.end(); ++iter) {
    if (iter->item->Type() == NGFragmentItem::kLine) {
      iter->offset.block_offset += delta;
      std::advance(iter, iter->item->DescendantsCount() - 1);
      DCHECK_LE(iter, items_.end());
      continue;
    }
    iter->offset.block_offset += delta;
  }
}

absl::optional<PhysicalSize> NGFragmentItemsBuilder::ToFragmentItems(
    const PhysicalSize& outer_size,
    void* data) {
  DCHECK(text_content_);
  ConvertToPhysical(outer_size);
  absl::optional<PhysicalSize> new_size;
  if (node_.IsSvgText()) {
    new_size = SvgTextLayoutAlgorithm(node_, GetWritingMode())
                   .Layout(TextContent(false), items_);
  }
  new (data) NGFragmentItems(this);
  return new_size;
}

}  // namespace blink
