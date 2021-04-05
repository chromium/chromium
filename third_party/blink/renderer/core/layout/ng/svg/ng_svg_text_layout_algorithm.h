// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_SVG_NG_SVG_TEXT_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_SVG_NG_SVG_TEXT_LAYOUT_ALGORITHM_H_

#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items_builder.h"

namespace blink {

struct SVGCharacterData;

class NGSVGTextLayoutAlgorithm {
  STACK_ALLOCATED();

 public:
  NGSVGTextLayoutAlgorithm(NGInlineNode node, WritingMode writing_mode);

  // Apply SVG specific text layout algorithm to |items|.
  // Text items in |items| will be converted to kSVGText type.
  void Layout(NGFragmentItemsBuilder::ItemWithOffsetList& items);

 private:
  // Returns false if we should skip the following steps.
  bool Setup(wtf_size_t approximate_count);
  void SetFlags(const NGFragmentItemsBuilder::ItemWithOffsetList& items);
  void ResolveCharacterPositioning(
      const LayoutObject& layout_object,
      const NGFragmentItemsBuilder::ItemWithOffsetList& items,
      bool in_text_path,
      wtf_size_t& index,
      Vector<SVGCharacterData>& resolve);
  void AdjustPositionsDxDy(
      const NGFragmentItemsBuilder::ItemWithOffsetList& items,
      Vector<SVGCharacterData>& resolve);
  void AdjustPositionsXY(
      const NGFragmentItemsBuilder::ItemWithOffsetList& items,
      const Vector<SVGCharacterData>& resolve);

  float ScalingFactorAt(const NGFragmentItemsBuilder::ItemWithOffsetList& items,
                        wtf_size_t addressable_index) const;

  NGInlineNode inline_node_;

  // This data member represents the number of addressable characters in the
  // target IFC. It's similar to "count" defined in the specification.
  wtf_size_t addressable_count_;

  // "horizontal" flag defined in the specification.
  bool horizontal_;

  struct NGSVGPerCharacterInfo {
    base::Optional<float> x;
    base::Optional<float> y;
    base::Optional<float> rotate;
    bool hidden = false;
    bool middle = false;
    bool anchored_chunk = false;
    wtf_size_t item_index = WTF::kNotFound;
  };
  // This data member represents "result" defined in the specification, but it
  // contains only addressable characters.
  Vector<NGSVGPerCharacterInfo> result_;

  // This data member represents "CSS_positions" defined in the specification,
  // but it contains only addressable characters.
  Vector<FloatPoint> css_positions_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_SVG_NG_SVG_TEXT_LAYOUT_ALGORITHM_H_
