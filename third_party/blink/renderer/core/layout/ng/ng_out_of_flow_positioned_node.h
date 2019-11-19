// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_OUT_OF_FLOW_POSITIONED_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_OUT_OF_FLOW_POSITIONED_NODE_H_

#include "third_party/blink/renderer/core/core_export.h"

#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_static_position.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"

namespace blink {

// A physical out-of-flow positioned-node is an element with the style
// "postion: absolute" or "position: fixed" which hasn't been bubbled up to its
// containing block yet, (e.g. an element with "position: relative"). As soon
// as a positioned-node reaches its containing block, it gets placed, and
// doesn't bubble further up the tree.
//
// This needs its static position [1] to be placed correctly in its containing
// block.
//
// This is struct is allowed to be stored/persisted.
//
// [1] https://www.w3.org/TR/CSS2/visudet.html#abs-non-replaced-width
struct CORE_EXPORT NGPhysicalOutOfFlowPositionedNode {
  NGBlockNode node;
  NGPhysicalStaticPosition static_position;
  // Continuation root of the optional inline container.
  const LayoutInline* inline_container;

  NGPhysicalOutOfFlowPositionedNode(
      NGBlockNode node,
      NGPhysicalStaticPosition static_position,
      const LayoutInline* inline_container = nullptr)
      : node(node),
        static_position(static_position),
        inline_container(inline_container) {
    DCHECK(!inline_container ||
           inline_container == inline_container->ContinuationRoot());
  }
};

// The logical version of above. It is used within a an algorithm pass (within
// an |NGContainerFragmentBuilder|), and its logical coordinate system is wrt.
// the container builder's writing-mode.
//
// It is *only* used within an algorithm pass, (it is temporary, and should not
// be stored/persisted).
struct NGLogicalOutOfFlowPositionedNode {
  NGBlockNode node;
  NGLogicalStaticPosition static_position;
  // Continuation root of the optional inline container.
  const LayoutInline* inline_container;

  NGLogicalOutOfFlowPositionedNode(
      NGBlockNode node,
      NGLogicalStaticPosition static_position,
      const LayoutInline* inline_container = nullptr)
      : node(node),
        static_position(static_position),
        inline_container(inline_container) {
    DCHECK(!inline_container ||
           inline_container == inline_container->ContinuationRoot());
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_OUT_OF_FLOW_POSITIONED_NODE_H_
