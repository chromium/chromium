// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_NODE_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_NODE_DATA_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

template <typename OffsetMappingBuilder>
class NGInlineItemsBuilderTemplate;

// Data which is required for inline nodes.
struct CORE_EXPORT NGInlineNodeData : NGInlineItemsData {
 public:
  bool IsBidiEnabled() const { return is_bidi_enabled_; }
  TextDirection BaseDirection() const {
    return static_cast<TextDirection>(base_direction_);
  }

  bool CanUseFastEditing() const { return can_use_fast_editing_; }
  bool HasLineEvenIfEmpty() const { return has_line_even_if_empty_; }
  bool HasRuby() const { return has_ruby_; }
  bool IsEmptyInline() const { return is_empty_inline_; }

  bool IsBlockLevel() const { return is_block_level_; }

  const NGInlineItemsData& ItemsData(bool is_first_line) const {
    return !is_first_line || !first_line_items_
               ? (const NGInlineItemsData&)*this
               : *first_line_items_;
  }

 private:
  void SetBaseDirection(TextDirection direction) {
    base_direction_ = static_cast<unsigned>(direction);
  }

  friend class NGInlineItemsBuilderTest;
  friend class NGInlineNode;
  friend class NGInlineNodeLegacy;
  friend class NGInlineNodeForTest;
  friend class NGOffsetMappingTest;

  template <typename OffsetMappingBuilder>
  friend class NGInlineItemsBuilderTemplate;

  // Items to use for the first line, when the node has :first-line rules.
  //
  // Items have different ComputedStyle, and may also have different
  // text_content and ShapeResult if 'text-transform' is applied or fonts are
  // different.
  std::unique_ptr<NGInlineItemsData> first_line_items_;

  unsigned is_bidi_enabled_ : 1;
  unsigned base_direction_ : 1;  // TextDirection

  // True if we can use fast editing path.
  unsigned can_use_fast_editing_ : 1;

  // True if there are no inline item items and the associated block is root
  // editable element or having "-internal-empty-line-height:fabricated",
  // e.g. <div contenteditable></div>, <input type=button value="">
  unsigned has_line_even_if_empty_ : 1;

  // The node contains <ruby>.
  unsigned has_ruby_ : 1;

  // We use this flag to determine if the inline node is empty, and will
  // produce a single zero block-size line box. If the node has text, atomic
  // inlines, open/close tags with margins/border/padding this will be false.
  unsigned is_empty_inline_ : 1;

  // We use this flag to determine if we have *only* floats, and OOF-positioned
  // children. If so we consider them block-level, and run the
  // |NGBlockLayoutAlgorithm| instead of the |NGInlineLayoutAlgorithm|. This is
  // done to pick up block-level static-position behaviour.
  unsigned is_block_level_ : 1;

  // True if changes to an item may affect different layout of earlier lines.
  // May not be able to use line caches even when the line or earlier lines are
  // not dirty.
  unsigned changes_may_affect_earlier_lines_ : 1;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_NODE_DATA_H_
