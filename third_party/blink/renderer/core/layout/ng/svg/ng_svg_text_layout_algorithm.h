// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_SVG_NG_SVG_TEXT_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_SVG_NG_SVG_TEXT_LAYOUT_ALGORITHM_H_

#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items_builder.h"

namespace blink {

class NGSVGTextLayoutAlgorithm {
  STACK_ALLOCATED();

 public:
  NGSVGTextLayoutAlgorithm(NGInlineNode node, WritingMode writing_mode);

  // Apply SVG specific text layout algorithm to |items|.
  // Text items in |items| will be converted to kSVGText type.
  void Layout(NGFragmentItemsBuilder::ItemWithOffsetList& items);

 private:
  NGInlineNode inline_node_;

  // "horizontal" flag defined in the specification.
  bool horizontal_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_SVG_NG_SVG_TEXT_LAYOUT_ALGORITHM_H_
