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
  // Returns false if we should skip the following steps.
  bool Setup();

  NGInlineNode inline_node_;

  // "count" defined in the specification.
  wtf_size_t count_;

  // "horizontal" flag defined in the specification.
  bool horizontal_;

  // "result" defined in the specification
  struct NGSVGPerCharacterInfo {
    base::Optional<float> x;
    base::Optional<float> y;
    base::Optional<float> rotate;
    bool hidden = false;
    bool addressable = true;
    bool middle = false;
    bool anchor_chunk = false;
    wtf_size_t item_index = WTF::kNotFound;
  };
  Vector<NGSVGPerCharacterInfo> result_;

  // "CSS_positions" defined in the specification.
  Vector<FloatPoint> css_positions_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_SVG_NG_SVG_TEXT_LAYOUT_ALGORITHM_H_
