// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_DOCUMENT_PART_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_DOCUMENT_PART_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/part_root.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class Document;
class DocumentFragment;
class Part;
class V8UnionDocumentOrDocumentFragment;

// Implementation of the DocumentPart class, which is part of the DOM Parts API.
// A DocumentPart holds the parts for a Document or DocumentFragment object.
class CORE_EXPORT DocumentPart : public PartRoot {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DocumentPart* Create(
      const V8UnionDocumentOrDocumentFragment* document) {
    return MakeGarbageCollected<DocumentPart>(document);
  }

  explicit DocumentPart(const V8UnionDocumentOrDocumentFragment* document);
  explicit DocumentPart(Document* document) : document_(document) {}
  explicit DocumentPart(DocumentFragment* document_fragment)
      : document_fragment_(document_fragment) {}
  DocumentPart(const DocumentPart&) = delete;
  ~DocumentPart() override;

  void Trace(Visitor*) const override;

  // DocumentPart API
  // TODO(crbug.com/1453291) Implement this method.
  PartRoot* root() const override { return nullptr; }
  DocumentPart* clone() const;
  // TODO(crbug.com/1453291) Implement this method.
  HeapVector<Member<Part>> getParts() override {
    return HeapVector<Member<Part>>();
  }

 private:
  Member<Document> document_;
  Member<DocumentFragment> document_fragment_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_DOCUMENT_PART_H_
