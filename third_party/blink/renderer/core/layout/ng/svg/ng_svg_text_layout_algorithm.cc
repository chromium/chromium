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
  // TODO(crbug.com/1179585): Implement this!
}

}  // namespace blink
