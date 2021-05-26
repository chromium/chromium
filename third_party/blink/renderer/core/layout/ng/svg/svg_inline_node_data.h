// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_SVG_SVG_INLINE_NODE_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_SVG_SVG_INLINE_NODE_DATA_H_

#include "third_party/blink/renderer/core/layout/ng/svg/ng_svg_character_data.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class LayoutObject;
class LayoutText;

struct SvgTextContentRange {
  DISALLOW_NEW();

  // This must be a LayoutSVGTextPath for |SVGInlineNodeData::
  // text_path_range_list|, and must be a LayoutObject for SVGTextContentElement
  // for SVGInlineNodeData::text_length_range_list
  const LayoutObject* layout_object;
  unsigned start_index;
  unsigned end_index;
};

}  // namespace blink

WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(blink::SvgTextContentRange)

namespace blink {

using SvgTextChunkOffsets = HashMap<const LayoutText*, Vector<unsigned>>;

// SVG-specific data stored in NGInlineNodeData.
struct SvgInlineNodeData final {
  Vector<std::pair<unsigned, NGSvgCharacterData>> character_data_list;
  Vector<SvgTextContentRange> text_length_range_list;
  Vector<SvgTextContentRange> text_path_range_list;
  SvgTextChunkOffsets chunk_offsets;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_SVG_SVG_INLINE_NODE_DATA_H_
