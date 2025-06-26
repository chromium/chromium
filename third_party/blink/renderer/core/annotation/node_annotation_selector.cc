// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/annotation/node_annotation_selector.h"

#include "base/functional/callback.h"
#include "third_party/blink/renderer/core/annotation/annotation_selector.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/range_in_flat_tree.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/dom_node_id.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

NodeAnnotationSelector::NodeAnnotationSelector(const DOMNodeId node_id)
    : node_id_(node_id) {}

void NodeAnnotationSelector::Trace(Visitor* visitor) const {
  AnnotationSelector::Trace(visitor);
}

String NodeAnnotationSelector::Serialize() const {
  StringBuilder builder;
  builder.AppendNumber(node_id_);
  return builder.ToString();
}

void NodeAnnotationSelector::FindRange(Range& search_range,
                                       SearchType type,
                                       FinishedCallback finished_cb) {
  Node* node = Node::FromDomNodeId(node_id_);
  if (!node || !node->isConnected()) {
    // If text not found call FinishedCallback with nullptr to match
    // requirements in AnnotationSelector::FinishedCallback.
    std::move(finished_cb).Run(nullptr);
    return;
  }

  // Make sure `node` is within `search_range`.
  if (!search_range.intersectsNode(node, ASSERT_NO_EXCEPTION)) {
    std::move(finished_cb).Run(nullptr);
    return;
  }

  const auto range_start = PositionInFlatTree::FirstPositionInNode(*node);
  const auto range_end = PositionInFlatTree::LastPositionInNode(*node);

  RangeInFlatTree* range = MakeGarbageCollected<RangeInFlatTree>(
      ToPositionInFlatTree(range_start), ToPositionInFlatTree(range_end));

  if (PlainText(range->ToEphemeralRange()).LengthWithStrippedWhiteSpace() > 0) {
    std::move(finished_cb).Run(range);
    return;
  }

  // If text not found call FinishedCallback with nullptr to match
  // requirements in AnnotationSelector::FinishedCallback.
  std::move(finished_cb).Run(nullptr);
}

}  // namespace blink
