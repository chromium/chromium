// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/opaque_range.h"

#include "third_party/blink/public/common/metrics/document_update_reason.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_list.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

OpaqueRange* OpaqueRange::Create(Document& document,
                                 TextControlElement* element,
                                 unsigned start_offset,
                                 unsigned end_offset) {
  UseCounter::Count(document, WebFeature::kOpaqueRange);
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
  CHECK(RuntimeEnabledFeatures::OpaqueRangeEnabled(
      document.GetExecutionContext()));
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

Document& OpaqueRange::OwnerDocument() const {
  return *owner_document_;
}

void OpaqueRange::UpdateOffsetsForTextChange(unsigned change_offset,
                                             unsigned deleted_count,
                                             unsigned inserted_count) {
  DCHECK(RuntimeEnabledFeatures::OpaqueRangeEnabled(
      owner_document_->GetExecutionContext()));
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
  EphemeralRange range = GetRangeForValue();
  // A null range is also collapsed, so this covers unresolvable ranges too.
  if (range.IsCollapsed()) {
    return MakeGarbageCollected<DOMRectList>();
  }
  auto* dom_range = CreateRange(range);
  DOMRectList* rects = dom_range->getClientRects();
  dom_range->Dispose();
  return rects;
}

DOMRect* OpaqueRange::getBoundingClientRect() const {
  EphemeralRange range = GetRangeForValue();
  if (range.IsNull()) {
    return DOMRect::Create();
  }
  auto* dom_range = CreateRange(range);
  DOMRect* rect = dom_range->getBoundingClientRect();
  dom_range->Dispose();
  return rect;
}

EphemeralRange OpaqueRange::GetRangeForValue() const {
  if (!element_ || !element_->isConnected()) {
    return EphemeralRange();
  }

  Document& doc = element_->GetDocument();
  doc.UpdateStyleAndLayout(DocumentUpdateReason::kJavaScript);

  if (!element_->GetLayoutObject()) {
    return EphemeralRange();
  }

  auto [start_node, start_local] =
      element_->ResolveValueOffset(start_offset_in_value_);
  auto [end_node, end_local] =
      element_->ResolveValueOffset(end_offset_in_value_);
  if (!start_node || !end_node) {
    return EphemeralRange();
  }

  return EphemeralRange(Position(start_node, start_local),
                        Position(end_node, end_local));
}

}  // namespace blink
