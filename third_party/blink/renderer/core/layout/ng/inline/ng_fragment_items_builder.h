// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_FRAGMENT_ITEMS_BUILDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_FRAGMENT_ITEMS_BUILDER_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_logical_line_item.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/text/writing_direction_mode.h"

namespace blink {

class NGFragmentItem;
class NGFragmentItems;
class NGInlineNode;

// This class builds |NGFragmentItems|.
//
// Once |NGFragmentItems| is built, it is immutable.
class CORE_EXPORT NGFragmentItemsBuilder {
  STACK_ALLOCATED();

 public:
  explicit NGFragmentItemsBuilder(WritingDirectionMode writing_direction);
  NGFragmentItemsBuilder(const NGInlineNode& node,
                         WritingDirectionMode writing_direction,
                         bool is_block_fragmented);
  ~NGFragmentItemsBuilder();

  WritingDirectionMode GetWritingDirection() const {
    return writing_direction_;
  }
  WritingMode GetWritingMode() const {
    return writing_direction_.GetWritingMode();
  }
  TextDirection Direction() const { return writing_direction_.Direction(); }

  wtf_size_t Size() const { return items_.size(); }

  // Returns true if we have any floating descendants which need to be
  // traversed during the float paint phase.
  bool HasFloatingDescendantsForPaint() const {
    return has_floating_descendants_for_paint_;
  }

  const String& TextContent(bool first_line) const {
    return UNLIKELY(first_line && first_line_text_content_)
               ? first_line_text_content_
               : text_content_;
  }

  // Adding a line is a three-pass operation, because |NGInlineLayoutAlgorithm|
  // creates and positions children within a line box, but its parent algorithm
  // positions the line box.
  //
  // 1. |AcquireLogicalLineItems| to get an instance of |NGLogicalLineItems|.
  // 2. Add items to |NGLogicalLineItems| and create |NGPhysicalFragment|,
  //    then associate them by |AssociateLogicalLineItems|.
  // 3. |AddLine| adds the |NGPhysicalLineBoxFragment|.
  //
  // |NGBlockLayoutAlgorithm| runs these phases in the order for each line. In
  // this case, one instance of |NGLogicalLineItems| is reused for all lines to
  // reduce memory allocations.
  //
  // Custom layout produces all line boxes first by running only 1 and 2 (in
  // |NGInlineLayoutAlgorithm|). Then after worklet determined the position and
  // the order of line boxes, it runs 3 for each line. In this case,
  // |NGFragmentItemsBuilder| allocates new instance for each line, and keeps
  // them alive until |AddLine|.
  NGLogicalLineItems* AcquireLogicalLineItems();
  void ReleaseCurrentLogicalLineItems();
  const NGLogicalLineItems& LogicalLineItems(
      const NGPhysicalLineBoxFragment&) const;
  void AssociateLogicalLineItems(NGLogicalLineItems* line_items,
                                 const NGPhysicalFragment& line_fragment);
  void AddLine(const NGPhysicalLineBoxFragment& line,
               const LogicalOffset& offset);

  // Add a list marker to the current line.
  void AddListMarker(const NGPhysicalBoxFragment& marker_fragment,
                     const LogicalOffset& offset);

  // See |AddPreviousItems| below.
  struct AddPreviousItemsResult {
    STACK_ALLOCATED();

   public:
    const NGInlineBreakToken* inline_break_token = nullptr;
    LayoutUnit used_block_size;
    wtf_size_t line_count = 0;
    bool succeeded = false;
  };

  // Add previously laid out |NGFragmentItems|.
  //
  // When |stop_at_dirty| is true, this function checks reusability of previous
  // items and stops copying before the first dirty line.
  AddPreviousItemsResult AddPreviousItems(
      const NGPhysicalBoxFragment& container,
      const NGFragmentItems& items,
      NGBoxFragmentBuilder* container_builder = nullptr,
      const NGFragmentItem* end_item = nullptr,
      wtf_size_t max_lines = 0);

  struct ItemWithOffset {
    DISALLOW_NEW();

   public:
    template <class... Args>
    explicit ItemWithOffset(const LogicalOffset& offset, Args&&... args)
        : item(std::forward<Args>(args)...), offset(offset) {}

    const NGFragmentItem& operator*() const { return item; }
    const NGFragmentItem* operator->() const { return &item; }

    void Trace(Visitor* visitor) const { visitor->Trace(item); }

    NGFragmentItem item;
    LogicalOffset offset;
  };

  // Give an inline size, the allocation of this vector is hot. "128" is
  // heuristic. Usually 10-40, some wikipedia pages have >64 items.
  using ItemWithOffsetList = HeapVector<ItemWithOffset, 128>;

  // Find |LogicalOffset| of the first |NGFragmentItem| for |LayoutObject|.
  absl::optional<LogicalOffset> LogicalOffsetFor(const LayoutObject&) const;

  // Moves all the |NGFragmentItem|s by |offset| in the block-direction.
  void MoveChildrenInBlockDirection(LayoutUnit offset);

  // Converts the |NGFragmentItem| vector to the physical coordinate space and
  // returns the result. This should only be used for determining the inline
  // containing block geometry for OOF-positioned nodes.
  //
  // Once this method has been called, new items cannot be added.
  const ItemWithOffsetList& Items(const PhysicalSize& outer_size);

  // Build a |NGFragmentItems|. The builder cannot build twice because data set
  // to this builder may be cleared.
  //
  // This function returns new size of the container if the container is an
  // SVG <text>.
  absl::optional<PhysicalSize> ToFragmentItems(const PhysicalSize& outer_size,
                                               void* data);

 private:
  void MoveCurrentLogicalLineItemsToMap();

  void AddItems(NGLogicalLineItem* child_begin, NGLogicalLineItem* child_end);

  void ConvertToPhysical(const PhysicalSize& outer_size);

  ItemWithOffsetList items_;
  String text_content_;
  String first_line_text_content_;

  // Keeps children of a line until the offset is determined. See |AddLine|.
  NGLogicalLineItems* current_line_items_ = nullptr;
  const NGPhysicalFragment* current_line_fragment_ = nullptr;

  HeapHashMap<Member<const NGPhysicalFragment>, Member<NGLogicalLineItems>>
      line_items_map_;
  NGLogicalLineItems* const line_items_pool_ =
      MakeGarbageCollected<NGLogicalLineItems>();

  NGInlineNode node_;

  WritingDirectionMode writing_direction_;

  bool has_floating_descendants_for_paint_ = false;
  bool is_converted_to_physical_ = false;
  bool is_line_items_pool_acquired_ = false;

  friend class NGFragmentItems;
};

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(
    blink::NGFragmentItemsBuilder::ItemWithOffset)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_FRAGMENT_ITEMS_BUILDER_H_
