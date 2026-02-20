// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/opaque_range.h"

#include "third_party/blink/public/common/metrics/document_update_reason.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_list.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

OpaqueRange* OpaqueRange::Create(Document& document,
                                 TextControlElement* element,
                                 unsigned start_offset,
                                 unsigned end_offset) {
  return MakeGarbageCollected<OpaqueRange>(document, element, start_offset,
                                           end_offset);
}

OpaqueRange::OpaqueRange(Document& document,
                         TextControlElement* element,
                         unsigned start_offset,
                         unsigned end_offset)
    : owner_document_(&document),
      element_(element),
      start_offset_in_value_(start_offset),
      end_offset_in_value_(end_offset) {
  CHECK(RuntimeEnabledFeatures::OpaqueRangeEnabled());
  element->RegisterOpaqueRange(this);
}

void OpaqueRange::Trace(Visitor* visitor) const {
  visitor->Trace(owner_document_);
  visitor->Trace(element_);
  ScriptWrappable::Trace(visitor);
}

unsigned OpaqueRange::startOffset() const {
  return start_offset_in_value_;
}

unsigned OpaqueRange::endOffset() const {
  return end_offset_in_value_;
}

bool OpaqueRange::collapsed() const {
  return start_offset_in_value_ == end_offset_in_value_;
}

bool OpaqueRange::IsStaticRange() const {
  return false;
}

Document& OpaqueRange::OwnerDocument() const {
  return *owner_document_;
}

void OpaqueRange::UpdateOffsetsForTextChange(unsigned change_offset,
                                             unsigned deleted_count,
                                             unsigned inserted_count) {
  DCHECK(RuntimeEnabledFeatures::OpaqueRangeEnabled());
  if (!element_ || (deleted_count == 0 && inserted_count == 0)) {
    return;
  }

  // State before the change.
  const unsigned pre_start = start_offset_in_value_;
  const unsigned pre_end = end_offset_in_value_;

  // Special case: pure insertion handling (no deletions) to match DOM Range
  // behavior.
  // A collapsed caret stays before inserted text, insertion at the range start
  // extends it, and insertion at the end leaves it unchanged.
  if (deleted_count == 0 && inserted_count > 0) {
    // Collapsed insertion: caret remains before new text.
    if (pre_start == pre_end && change_offset == pre_start) {
      return;
    }
    // Insertion at range start: extend the end to include the new text.
    if (pre_start != pre_end && change_offset == pre_start) {
      end_offset_in_value_ = pre_end + inserted_count;
      return;
    }
    // Insertion at range end: leave the range unchanged.
    if (pre_start != pre_end && change_offset == pre_end) {
      return;
    }
  }

  // Special case: deletion of the entire old value collapses to [0,0].
  if (deleted_count > 0 && change_offset == 0 && pre_end <= deleted_count) {
    start_offset_in_value_ = 0;
    end_offset_in_value_ = 0;
    return;
  }

  const unsigned change_end = change_offset + deleted_count;
  auto calculate_new_offset = [&](unsigned pos) -> unsigned {
    // Case 1: Position is before the change, so it remains unchanged.
    if (pos < change_offset) {
      return pos;
    }

    // Case 2: Position is inside the deleted region, so move it to the start of
    // the change.
    if (pos < change_end) {
      return change_offset;
    }

    // Case 3: Position is after the change, so shift it by the net difference.
    return pos - deleted_count + inserted_count;
  };

  // Clamp to the current value length and ensure start does not exceed end.
  const unsigned value_length = element_->Value().length();
  unsigned new_start = std::min(calculate_new_offset(pre_start), value_length);
  unsigned new_end = std::min(calculate_new_offset(pre_end), value_length);

  // Auto-collapse to higher index if needed.
  if (new_start > new_end) {
    new_end = new_start;
  }

  start_offset_in_value_ = new_start;
  end_offset_in_value_ = new_end;
}

void OpaqueRange::disconnect() {
  if (element_) {
    element_->UnregisterOpaqueRange(this);
    element_ = nullptr;
  }
  start_offset_in_value_ = 0;
  end_offset_in_value_ = 0;
}

DOMRectList* OpaqueRange::getClientRects() const {
  Range* range = BuildValueGeometryContext();
  if (!range || range->collapsed()) {
    return MakeGarbageCollected<DOMRectList>();
  }
  return range->getClientRects();
}

DOMRect* OpaqueRange::getBoundingClientRect() const {
  Range* range = BuildValueGeometryContext();
  if (!range) {
    return DOMRect::Create();
  }
  return range->getBoundingClientRect();
}

Range* OpaqueRange::BuildValueGeometryContext() const {
  if (!element_ || !element_->isConnected()) {
    return nullptr;
  }

  Document& doc = element_->GetDocument();
  doc.UpdateStyleAndLayout(DocumentUpdateReason::kJavaScript);

  if (!element_->GetLayoutObject()) {
    return nullptr;
  }

  Element* inner = element_->InnerEditorElement();
  if (!inner) {
    return nullptr;
  }

  Text* text_node = nullptr;
  // The element's text content is rendered in shadow-DOM text nodes under the
  // inner editor. Iterate the children to locate the first text node.
  // TODO(crbug.com/482337697): Aggregate text when it spans multiple nodes.
  for (Node* n = inner->firstChild(); n; n = n->nextSibling()) {
    if (auto* t = DynamicTo<Text>(n)) {
      text_node = t;
      break;
    }
  }
  if (!text_node) {
    return nullptr;
  }

  const unsigned len = text_node->data().length();
  const unsigned start = std::min(start_offset_in_value_, len);
  const unsigned end = std::min(end_offset_in_value_, len);

  Range* range = Range::Create(doc);
  range->setStart(Position(text_node, start));
  range->setEnd(Position(text_node, end));
  return range;
}

}  // namespace blink
