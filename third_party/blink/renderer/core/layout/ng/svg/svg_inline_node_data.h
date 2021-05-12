// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_SVG_SVG_INLINE_NODE_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_SVG_SVG_INLINE_NODE_DATA_H_

#include "third_party/blink/renderer/core/layout/ng/svg/ng_svg_character_data.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_text_path.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

struct SVGTextPathRange {
  DISALLOW_NEW();

  Member<const LayoutSVGTextPath> layout_svg_text_path;
  unsigned start_index;
  unsigned end_index;

  void Trace(Visitor* visitor) const { visitor->Trace(layout_svg_text_path); }
};

}  // namespace blink

WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(blink::SVGTextPathRange)

namespace blink {

// SVG-specific data stored in NGInlineNodeData.
struct SVGInlineNodeData final : public GarbageCollected<SVGInlineNodeData> {
  Vector<std::pair<unsigned, NGSVGCharacterData>> character_data_list;
  HeapVector<SVGTextPathRange> text_path_range_list;

  void Trace(Visitor* visitor) const { visitor->Trace(text_path_range_list); }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_SVG_SVG_INLINE_NODE_DATA_H_
