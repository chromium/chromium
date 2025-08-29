// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/form_control_range.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

FormControlRange* FormControlRange::Create(Document& document) {
  return MakeGarbageCollected<FormControlRange>(document);
}

FormControlRange::FormControlRange(Document& document)
    : owner_document_(&document) {}

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

}  // namespace blink
