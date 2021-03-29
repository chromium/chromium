// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/svg/ng_svg_text_layout_algorithm.h"

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
  // 1. Setup
  if (!Setup())
    return;

  // TODO(crbug.com/1179585): Implement the following steps.
}

bool NGSVGTextLayoutAlgorithm::Setup() {
  // 1.2. Let count be the number of DOM characters within the ‘text’ element's
  // subtree.
  count_ = inline_node_.GetDOMNode()->textContent().length();

  // 1.3. Let result be an array of length count whose entries contain the
  // per-character information described above.
  // ... If result is empty, then return result.
  if (count_ == 0)
    return false;
  result_.resize(count_);

  // 1.4. Let CSS_positions be an array of length count whose entries will be
  // filled with the x and y positions of the corresponding typographic
  // character in root. The array entries are initialized to (0, 0).
  css_positions_.resize(count_);
  return true;
}

}  // namespace blink
