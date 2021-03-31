// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/svg/ng_svg_text_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"

namespace blink {

// See https://svgwg.org/svg2-draft/text.html#TextLayoutAlgorithm

NGSVGTextLayoutAlgorithm::NGSVGTextLayoutAlgorithm(NGInlineNode node,
                                                   WritingMode writing_mode)
    : inline_node_(node),
      // 1.5. Let "horizontal" be a flag, true if the writing mode of ‘text’
      // is horizontal, false otherwise.
      horizontal_(IsHorizontalWritingMode(writing_mode)) {
  DCHECK(node.IsSVGText());
}

void NGSVGTextLayoutAlgorithm::Layout(
    NGFragmentItemsBuilder::ItemWithOffsetList& items) {
  // https://svgwg.org/svg2-draft/text.html#TextLayoutAlgorithm
  //
  // The major difference from the algorithm in the specification:
  // We handle only addressable characters. The size of "result",
  // "CSS_positions", and "resolved" is the number of addressable characters.

  // 1. Setup
  if (!Setup(items.size()))
    return;

  // 2. Set flags and assign initial positions
  SetFlags(items);

  // TODO(crbug.com/1179585): Implement the following steps.
}

bool NGSVGTextLayoutAlgorithm::Setup(wtf_size_t approximate_count) {
  // 1.2. Let count be the number of DOM characters within the ‘text’ element's
  // subtree.
  // ==> We don't use |count|. We set |addressable_count_| in the step 2.

  // 1.3. Let result be an array of length count whose entries contain the
  // per-character information described above.
  // ... If result is empty, then return result.
  if (approximate_count == 0)
    return false;
  // ==> We don't fill |result| here. We do it in the step 2.
  result_.ReserveCapacity(approximate_count);

  // 1.4. Let CSS_positions be an array of length count whose entries will be
  // filled with the x and y positions of the corresponding typographic
  // character in root. The array entries are initialized to (0, 0).
  // ==> We don't fill |CSS_positions| here. We do it in the step 2.
  css_positions_.ReserveCapacity(approximate_count);
  return true;
}

void NGSVGTextLayoutAlgorithm::SetFlags(
    const NGFragmentItemsBuilder::ItemWithOffsetList& items) {
  // This function collects information per an "addressable" character in DOM
  // order. So we need to access NGFragmentItems in the logical order.
  Vector<wtf_size_t> sorted_item_indexes;
  sorted_item_indexes.ReserveCapacity(items.size());
  for (wtf_size_t i = 0; i < items.size(); ++i) {
    if (items[i]->Type() == NGFragmentItem::kText)
      sorted_item_indexes.push_back(i);
  }
  if (inline_node_.IsBidiEnabled()) {
    std::sort(sorted_item_indexes.data(),
              sorted_item_indexes.data() + sorted_item_indexes.size(),
              [&](wtf_size_t a, wtf_size_t b) {
                return items[a]->StartOffset() < items[b]->StartOffset();
              });
  }

  bool found_first_character = false;
  for (wtf_size_t i : sorted_item_indexes) {
    NGSVGPerCharacterInfo info;
    info.item_index = i;
    // 2.3. If the character at index i corresponds to a typographic
    // character at the beginning of a line, then set the "anchored chunk"
    // flag of result[i] to true.
    if (!found_first_character) {
      found_first_character = true;
      info.anchored_chunk = true;
    }
    // 2.4. If addressable is true and middle is false then set
    // CSS_positions[i] to the position of the corresponding typographic
    // character as determined by the CSS renderer.
    const NGFragmentItem& item = *items[info.item_index];
    PhysicalOffset offset = item.OffsetInContainerFragment();
    const auto* layout_svg_inline =
        To<LayoutSVGInlineText>(item.GetLayoutObject());
    LayoutUnit ascent = layout_svg_inline->ScaledFont()
                            .PrimaryFont()
                            ->GetFontMetrics()
                            .FixedAscent();
    // TODO(crbug.com/1179585): Supports vertical flow.
    css_positions_.push_back(FloatPoint(offset.left, offset.top + ascent));
    result_.push_back(info);

    // 2.2. Set middle to true if the character at index i is the second or
    // later character that corresponds to a typographic character.
    for (unsigned text_offset = item.StartOffset() + 1;
         text_offset < item.EndOffset(); ++text_offset) {
      NGSVGPerCharacterInfo middle_info;
      middle_info.middle = true;
      middle_info.item_index = info.item_index;
      result_.push_back(middle_info);
      css_positions_.push_back(css_positions_.back());
    }
  }
  addressable_count_ = result_.size();
}

}  // namespace blink
