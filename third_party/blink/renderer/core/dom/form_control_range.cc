// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/form_control_range.h"

#include "third_party/blink/public/common/metrics/document_update_reason.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_list.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

FormControlRange* FormControlRange::Create(Document& document) {
  return MakeGarbageCollected<FormControlRange>(document);
}

FormControlRange::FormControlRange(Document& document)
    : owner_document_(&document) {
  CHECK(RuntimeEnabledFeatures::FormControlRangeEnabled());
}

void FormControlRange::Trace(Visitor* visitor) const {
  visitor->Trace(owner_document_);
  visitor->Trace(form_control_);
  ScriptWrappable::Trace(visitor);
}

Node* FormControlRange::startContainer() const {
  return form_control_;
}

Node* FormControlRange::endContainer() const {
  return form_control_;
}

unsigned FormControlRange::startOffset() const {
  return start_offset_in_value_;
}

unsigned FormControlRange::endOffset() const {
  return end_offset_in_value_;
}

bool FormControlRange::collapsed() const {
  return start_offset_in_value_ == end_offset_in_value_;
}

bool FormControlRange::IsStaticRange() const {
  return false;
}

Document& FormControlRange::OwnerDocument() const {
  return *owner_document_;
}

void FormControlRange::setFormControlRange(Node* element,
                                           unsigned start_offset,
                                           unsigned end_offset,
                                           ExceptionState& exception_state) {
  // Validate element is a supported text control.
  auto* text_control = DynamicTo<TextControlElement>(element);
  if (!text_control) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "Element must be an <input> or a <textarea>.");
    return;
  }

  // For <input>, ensure it supports the Selection API.
  if (auto* input_element = DynamicTo<HTMLInputElement>(element)) {
    if (!input_element->InputSupportsSelectionAPI()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kNotSupportedError,
          "<input> element must be of a text field type: text, search, url, "
          "tel, or password.");
      return;
    }
  }

  const String value = text_control->Value();
  if (start_offset > value.length() || end_offset > value.length()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        "Start or end offset exceeds value length.");
    return;
  }

  // Auto-collapse backwards ranges to match Range behavior.
  if (start_offset > end_offset) {
    end_offset = start_offset;
  }

  // Rebind to the new control if changed and update registration to receive
  // value mutation notifications.
  if (form_control_ != text_control) {
    if (form_control_) {
      form_control_->UnregisterFormControlRange(this);
    }
    form_control_ = text_control;
    form_control_->RegisterFormControlRange(this);
  }
  start_offset_in_value_ = start_offset;
  end_offset_in_value_ = end_offset;
}

String FormControlRange::toString() const {
  if (!form_control_) {
    return g_empty_string;
  }

  const String value = form_control_->Value();
  const unsigned len = value.length();
  const unsigned end_offset = std::min(end_offset_in_value_, len);
  if (start_offset_in_value_ >= end_offset) {
    return g_empty_string;
  }

  return value.Substring(start_offset_in_value_,
                         end_offset - start_offset_in_value_);
}

void FormControlRange::UpdateOffsetsForTextChange(unsigned change_offset,
                                                  unsigned deleted_count,
                                                  unsigned inserted_count) {
  DCHECK(RuntimeEnabledFeatures::FormControlRangeEnabled());
  if (!form_control_ || (deleted_count == 0 && inserted_count == 0)) {
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
  const unsigned value_length = form_control_->Value().length();
  unsigned new_start = std::min(calculate_new_offset(pre_start), value_length);
  unsigned new_end = std::min(calculate_new_offset(pre_end), value_length);

  // Auto-collapse to higher index if needed.
  if (new_start > new_end) {
    new_end = new_start;
  }

  start_offset_in_value_ = new_start;
  end_offset_in_value_ = new_end;
}

DOMRectList* FormControlRange::getClientRects() const {
  Range* range = BuildValueGeometryContext();
  if (!range || range->collapsed()) {
    return MakeGarbageCollected<DOMRectList>();
  }
  return range->getClientRects();
}

DOMRect* FormControlRange::getBoundingClientRect() const {
  Range* range = BuildValueGeometryContext();
  if (!range) {
    return DOMRect::Create();
  }
  return range->getBoundingClientRect();
}

Range* FormControlRange::BuildValueGeometryContext() const {
  if (!form_control_ || !form_control_->isConnected()) {
    return nullptr;
  }

  Document& doc = form_control_->GetDocument();
  doc.UpdateStyleAndLayout(DocumentUpdateReason::kJavaScript);

  if (!form_control_->GetLayoutObject()) {
    return nullptr;
  }

  Element* inner = form_control_->InnerEditorElement();
  if (!inner) {
    return nullptr;
  }

  Text* text_node = nullptr;
  // `.value` is copied into shadow-DOM text nodes under the inner editor, but
  // there is no accessor yet. Iterate the children to locate that copy. The
  // first text node is returned for now.
  // TODO: Aggregate copied text when it spans multiple nodes.
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
