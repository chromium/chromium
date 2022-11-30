// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/static_range.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_static_range_init.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

StaticRange::StaticRange(Document& document,
                         Node* start_container,
                         unsigned start_offset,
                         Node* end_container,
                         unsigned end_offset)
    : owner_document_(document),
      start_container_(start_container),
      start_offset_(start_offset),
      end_container_(end_container),
      end_offset_(end_offset) {}

// static
StaticRange* StaticRange::Create(const EphemeralRange& range) {
  DCHECK(!range.IsNull());
  return MakeGarbageCollected<StaticRange>(
      range.GetDocument(), range.StartPosition().ComputeContainerNode(),
      range.StartPosition().ComputeOffsetInContainerNode(),
      range.EndPosition().ComputeContainerNode(),
      range.EndPosition().ComputeOffsetInContainerNode());
}

StaticRange* StaticRange::Create(Document& document,
                                 const StaticRangeInit* static_range_init,
                                 ExceptionState& exception_state) {
  DCHECK(static_range_init);

  if (static_range_init->startContainer()->IsDocumentTypeNode() ||
      static_range_init->startContainer()->IsAttributeNode() ||
      static_range_init->endContainer()->IsDocumentTypeNode() ||
      static_range_init->endContainer()->IsAttributeNode()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidNodeTypeError,
        "Neither startContainer nor endContainer can be a DocumentType or "
        "Attribute node.");
  }

  return MakeGarbageCollected<StaticRange>(
      document, static_range_init->startContainer(),
      static_range_init->startOffset(), static_range_init->endContainer(),
      static_range_init->endOffset());
}

bool StaticRange::IsValid() const {
  if (dom_tree_version_for_is_valid_ == owner_document_->DomTreeVersion())
    return is_valid_;
  dom_tree_version_for_is_valid_ = owner_document_->DomTreeVersion();

  // The full list of checks is:
  //  1) The start offset is between 0 and the start container’s node length
  //     (inclusive).
  //  2) The end offset is between 0 and the end container’s node length
  //     (inclusive).
  //  3) The start and end containers of the static range are in the same DOM
  //     tree.
  //  4) The position of the start boundary point is before or equal to the
  //     position of the end boundary point.
  is_valid_ =
      start_offset_ <= AbstractRange::LengthOfContents(start_container_) &&
      end_offset_ <= AbstractRange::LengthOfContents(end_container_) &&
      !HasDifferentRootContainer(start_container_, end_container_) &&
      ComparePositionsInDOMTree(start_container_, start_offset_, end_container_,
                                end_offset_) <= 0;

  return is_valid_;
}

Range* StaticRange::toRange(ExceptionState& exception_state) const {
  Range* range = Range::Create(*owner_document_.Get());
  // Do the offset checking.
  range->setStart(start_container_, start_offset_, exception_state);
  range->setEnd(end_container_, end_offset_, exception_state);
  return range;
}

void StaticRange::Trace(Visitor* visitor) const {
  visitor->Trace(owner_document_);
  visitor->Trace(start_container_);
  visitor->Trace(end_container_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
