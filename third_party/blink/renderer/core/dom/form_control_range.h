// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_FORM_CONTROL_RANGE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_FORM_CONTROL_RANGE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/abstract_range.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class Document;
class Node;
class TextControlElement;

// A live range over a single text-control element (<input> or <textarea>).
// Endpoints are exposed as (host element, UTF-16 value indices) rather than
// internal shadow-DOM nodes.
class CORE_EXPORT FormControlRange final : public AbstractRange {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static FormControlRange* Create(Document&);

  explicit FormControlRange(Document&);

  void Trace(Visitor* visitor) const override;

  // AbstractRange implementation:
  // For FormControlRange, both containers are the host form control element.
  Node* startContainer() const override;
  Node* endContainer() const override;

  // Offsets are indices into the host's .value
  unsigned startOffset() const override;
  unsigned endOffset() const override;

  bool collapsed() const override;
  bool IsStaticRange() const override;
  Document& OwnerDocument() const override;

 private:
  Member<Document> owner_document_;

  // The observed form control.
  Member<TextControlElement> form_control_;

  // Offsets into the form control’s `.value` for the range endpoints; updated
  // live on text edits.
  unsigned start_offset_in_value_ = 0;
  unsigned end_offset_in_value_ = 0;
};
}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_FORM_CONTROL_RANGE_H_
