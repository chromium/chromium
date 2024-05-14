// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/abstract_range.h"

#include "third_party/blink/renderer/core/dom/character_data.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/node.h"

namespace blink {

AbstractRange::AbstractRange() = default;
AbstractRange::~AbstractRange() = default;

bool AbstractRange::HasDifferentRootContainer(Node* start_root_container,
                                              Node* end_root_container) {
  return start_root_container->TreeRoot() != end_root_container->TreeRoot();
}

unsigned AbstractRange::LengthOfContents(const Node* node) {
  // This switch statement must be consistent with that of
  // Range::processContentsBetweenOffsets.
  switch (node->getNodeType()) {
    case Node::kTextNode:
    case Node::kCdataSectionNode:
    case Node::kCommentNode:
    case Node::kProcessingInstructionNode:
      return To<CharacterData>(node)->length();
    case Node::kElementNode:
    case Node::kDocumentNode:
    case Node::kDocumentFragmentNode:
      return To<ContainerNode>(node)->CountChildren();
    case Node::kAttributeNode:
    case Node::kDocumentTypeNode:
      return 0;
  }
  NOTREACHED_IN_MIGRATION();
  return 0;
}

}  // namespace blink
