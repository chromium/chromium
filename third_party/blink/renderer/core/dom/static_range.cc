// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/static_range.h"

#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/static_range_init.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

StaticRange::StaticRange(Document& document)
    : owner_document_(document),
      start_container_(document),
      start_offset_(0u),
      end_container_(document),
      end_offset_(0u) {}

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

void StaticRange::setStart(Node* container, unsigned offset) {
  start_container_ = container;
  start_offset_ = offset;
}

void StaticRange::setEnd(Node* container, unsigned offset) {
  end_container_ = container;
  end_offset_ = offset;
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
