// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_NODE_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_NODE_DATA_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_items_data.h"
#include "third_party/blink/renderer/core/layout/ng/svg/svg_inline_node_data.h"

namespace blink {

template <typename OffsetMappingBuilder>
class NGInlineItemsBuilderTemplate;

// Data which is required for inline nodes.
struct CORE_EXPORT NGInlineNodeData final : NGInlineItemsData {
 public:
  NGInlineNodeData() = default;
  bool IsBidiEnabled() const { return is_bidi_enabled_; }
  TextDirection BaseDirection() const {
    return static_cast<TextDirection>(base_direction_);
  }

  bool HasInitialLetterBox() const { return has_initial_letter_box_; }
  bool HasRuby() const { return has_ruby_; }

  bool IsBlockLevel() const { return is_block_level_; }

  const NGInlineItemsData& ItemsData(bool is_first_line) const {
    return !is_first_line || !first_line_items_
               ? (const NGInlineItemsData&)*this
               : *first_line_items_;
  }

  void Trace(Visitor* visitor) const override;

 private:
  void SetBaseDirection(TextDirection direction) {
    base_direction_ = static_cast<unsigned>(direction);
  }

  friend class NGInlineItemsBuilderTest;
  friend class NGInlineNode;
  friend class NGInlineNodeForTest;
  friend class NGOffsetMappingTest;

  template <typename OffsetMappingBuilder>
  friend class NGInlineItemsBuilderTemplate;

  // Items to use for the first line, when the node has :first-line rules.
  //
  // Items have different ComputedStyle, and may also have different
  // text_content and ShapeResult if 'text-transform' is applied or fonts are
  // different.
  Member<NGInlineItemsData> first_line_items_;

  Member<SvgInlineNodeData> svg_node_data_;

  unsigned is_bidi_enabled_ : 1;
  unsigned base_direction_ : 1;  // TextDirection

  // True if this node contains initial letter box. This value is used for
  // clearing. To control whether subsequent blocks overlap with initial
  // letter[1].
  //   ****** his node ends here.
  //     *    This text from subsequent block one.
  //     *    This text from subsequent block two.
  //     *    This text from subsequent block three.
  // [1] https://drafts.csswg.org/css-inline/#initial-letter-paragraphs
  unsigned has_initial_letter_box_ : 1;

  // The node contains <ruby>.
  unsigned has_ruby_ : 1;

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
