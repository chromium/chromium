// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_SUPPLEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_SUPPLEMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/document_transition/document_transition.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {
class DocumentTransition;

class CORE_EXPORT DocumentTransitionSupplement
    : public GarbageCollected<DocumentTransitionSupplement>,
      public Supplement<Document> {
 public:
  static const char kSupplementName[];

  // Supplement functionality.
  static DocumentTransitionSupplement* From(Document&);
  static DocumentTransitionSupplement* FromIfExists(Document&);

  static DocumentTransition* documentTransition(Document&);

  DocumentTransition* GetTransition();

  explicit DocumentTransitionSupplement(Document&);

  // GC functionality.
  void Trace(Visitor* visitor) const override;

 private:
  Member<DocumentTransition> transition_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_SUPPLEMENT_H_
