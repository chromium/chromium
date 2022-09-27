// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_SVG_NG_SVG_TEXT_LAYOUT_ATTRIBUTES_BUILDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_SVG_NG_SVG_TEXT_LAYOUT_ATTRIBUTES_BUILDER_H_

#include "third_party/blink/renderer/core/layout/ng/svg/ng_svg_character_data.h"
#include "third_party/blink/renderer/core/layout/ng/svg/svg_inline_node_data.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class LayoutBlockFlow;
class NGInlineItem;
class NGInlineNode;

// This class builds a list of <addressable character offset,
// its attribute values> for the specified SVG <text>.
//
// This is almost an implementation of '3. Resolve character positioning'
// in the algorithm [1]. However this runs during PrepareLayout() rather
// than during the SVG text layout algorithm because we'd like to use the
// result of this class in NGInlineNode::CollectInlines().
//
// Also, this is responsible to make lists of index ranges for <textPath> and
// textLength.
//
// [1] https://svgwg.org/svg2-draft/text.html#TextLayoutAlgorithm
class NGSvgTextLayoutAttributesBuilder final {
  STACK_ALLOCATED();

 public:
  explicit NGSvgTextLayoutAttributesBuilder(NGInlineNode ifc);

  void Build(const String& ifc_text_content,
             const HeapVector<NGInlineItem>& items);

  // This function can be called just once after Build().
  SvgInlineNodeData* CreateSvgInlineNodeData();

 private:
  LayoutBlockFlow* block_flow_;

  // The result of Build().
  // A list of a pair of addressable character index and an
  // NGSvgCharacterData. This is named 'resolved' because this is
  // the outcome of '3. Resolve character positioning'.
  Vector<std::pair<unsigned, NGSvgCharacterData>> resolved_;

  // The result of Build().
  // A list of a pair of start addressable character index and end
  // addressable character index (inclusive) for an SVGTextContentElement
  // with textLength.
  // This is used in "5. Apply ‘textLength’ attribute".
  HeapVector<SvgTextContentRange> text_length_range_list_;

  // The result of Build().
  // A list of a pair of start addressable character index and end
  // addressable character index (inclusive) for a <textPath>.
  // This is used in "8. Position on path".
  HeapVector<SvgTextContentRange> text_path_range_list_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_SVG_NG_SVG_TEXT_LAYOUT_ATTRIBUTES_BUILDER_H_
