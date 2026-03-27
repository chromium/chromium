// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_OPAQUE_RANGE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_OPAQUE_RANGE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/abstract_range.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class Document;
class DOMRect;
class DOMRectList;
class Node;
class Range;
class TextControlElement;

// An OpaqueRange is a live AbstractRange subtype whose boundary points
// reference encapsulated content within text-control elements (<input>,
// <textarea>). Containers return null to avoid exposing internal DOM
// structure. startOffset/endOffset are live UTF-16 code unit indices into the
// element's value. Created via createValueRange(start, end) on elements that
// support opaque ranges.
class CORE_EXPORT OpaqueRange final : public AbstractRange {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static OpaqueRange* Create(Document&,
                             TextControlElement*,
                             unsigned start_offset,
                             unsigned end_offset);

  OpaqueRange(Document&,
              TextControlElement*,
              unsigned start_offset,
              unsigned end_offset);

  void Trace(Visitor* visitor) const override;

  // AbstractRange implementation:
  // Containers return null to hide internal DOM structure.
  Node* startContainer() const override { return nullptr; }
  Node* endContainer() const override { return nullptr; }

  // Offsets are UTF-16 code unit indices into the element's text content.
  unsigned startOffset() const override;
  unsigned endOffset() const override;

  bool collapsed() const override;
  bool IsStaticRange() const override { return false; }
  bool IsOpaqueRange() const override { return true; }
  Document& OwnerDocument() const override;

  // Update after a text replacement at `change_offset`: removes
  // `deleted_count` code units and adds `inserted_count`.
  // `start_offset_in_value_` and `end_offset_in_value_` are adjusted to reflect
  // the edit and clamped to the element's current value.
  void UpdateOffsetsForTextChange(unsigned change_offset,
                                  unsigned deleted_count,
                                  unsigned inserted_count);

  // Detaches this range from its element, stopping live offset updates.
  // After calling disconnect(), startOffset/endOffset return 0 and
  // getClientRects()/getBoundingClientRect() return empty results.
  void disconnect();

  DOMRectList* getClientRects() const;
  DOMRect* getBoundingClientRect() const;

  // Returns the associated text control element, or nullptr if disconnected.
  TextControlElement* GetElement() const { return element_.Get(); }

  // Creates a DOM Range spanning the stored value offsets within the element's
  // inner editor. Forces style/layout update. Returns nullptr if the element is
  // disconnected, has no layout object, or has no inner editor.
  Range* BuildValueGeometryContext() const;

 private:
  Member<Document> owner_document_;

  // The observed text-control element.
  Member<TextControlElement> element_;

  // UTF-16 code unit offsets into the element's text content; updated live on
  // text edits.
  unsigned start_offset_in_value_ = 0;
  unsigned end_offset_in_value_ = 0;
};

template <>
struct DowncastTraits<OpaqueRange> {
  static bool AllowFrom(const AbstractRange& abstract_range) {
    return abstract_range.IsOpaqueRange();
  }
};
}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_OPAQUE_RANGE_H_
