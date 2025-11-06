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
class DOMRect;
class DOMRectList;
class ExceptionState;
class Node;
class Range;
class TextControlElement;

// A live range over a single text-control element (<input> or <textarea>).
// Endpoints are exposed as (host element, UTF-16 `value` indices) rather than
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

  // Offsets are indices into the host's `.value`.
  unsigned startOffset() const override;
  unsigned endOffset() const override;

  bool collapsed() const override;
  bool IsStaticRange() const override;
  Document& OwnerDocument() const override;

  // Sets the range on a <textarea> or text-supporting <input>.
  // Offsets are UTF-16 indices into element.value; throws on unsupported
  // elements or out-of-bounds offsets. Backwards ranges (start_offset >
  // end_offset) are auto-collapsed to [start_offset, start_offset] to match
  // standard Range behavior.
  void setFormControlRange(Node* element,
                           unsigned start_offset,
                           unsigned end_offset,
                           ExceptionState&);

  // Returns the substring of element.value; empty string if unset or invalid.
  String toString() const;

  // Update `form_control_` after a text replacement at `change_offset`: removes
  // `deleted_count` code units and adds `inserted_count`.
  // `start_offset_in_value_` and `end_offset_in_value_` are adjusted to reflect
  // the edit and clamped to the current value of `form_control_`.
  void UpdateOffsetsForTextChange(unsigned change_offset,
                                  unsigned deleted_count,
                                  unsigned inserted_count);

  DOMRectList* getClientRects() const;
  DOMRect* getBoundingClientRect() const;

 private:
  // Internal helper that prepares a DOM Range anchored to the inner editor
  // text node that holds a copy of the control's `.value`. Offsets are clamped
  // to the current text length. Returns nullptr if geometry is unavailable
  // (e.g. control unset or disconnected, missing inner editor, etc).
  Range* BuildValueGeometryContext() const;

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
