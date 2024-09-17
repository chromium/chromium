// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MASONRY_MASONRY_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MASONRY_MASONRY_NODE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/block_node.h"

namespace blink {

// Masonry specific extensions to `BlockNode`.
class CORE_EXPORT MasonryNode final : public BlockNode {
 public:
  explicit MasonryNode(LayoutBox* box) : BlockNode(box) {
    DCHECK(box);
    DCHECK(box->IsLayoutMasonry());
  }
};

template <>
struct DowncastTraits<MasonryNode> {
  static bool AllowFrom(const LayoutInputNode& node) {
    return node.IsMasonry();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MASONRY_MASONRY_NODE_H_
