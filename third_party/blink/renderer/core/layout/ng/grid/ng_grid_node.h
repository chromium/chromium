// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_NODE_H_

#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"

namespace blink {

// Grid specific extensions to NGBlockNode.
class CORE_EXPORT NGGridNode final : public NGBlockNode {
 public:
  explicit NGGridNode(LayoutBox* box) : NGBlockNode(box) {
    DCHECK(box && box->IsLayoutNGGrid());
  }
};

template <>
struct DowncastTraits<NGGridNode> {
  static bool AllowFrom(const NGLayoutInputNode& node) { return node.IsGrid(); }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_NODE_H_
