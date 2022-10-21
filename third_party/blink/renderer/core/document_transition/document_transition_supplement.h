// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_SUPPLEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_SUPPLEMENT_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_document_transition_callback.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/document_transition/document_transition.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {
class DocumentTransition;
class V8DocumentTransitionCallback;

class CORE_EXPORT DocumentTransitionSupplement
    : public GarbageCollected<DocumentTransitionSupplement>,
      public Supplement<Document>,
      public DocumentTransitionDirectiveStore {
 public:
  static const char kSupplementName[];

  // Supplement functionality.
  static DocumentTransitionSupplement* From(Document&);
  static DocumentTransitionSupplement* FromIfExists(const Document&);

  static DocumentTransition* createDocumentTransition(
      ScriptState*,
      Document&,
      V8DocumentTransitionCallback* callback,
      ExceptionState&);

  DocumentTransition* GetActiveTransition();

  explicit DocumentTransitionSupplement(Document&);
  virtual ~DocumentTransitionSupplement() = default;

  // GC functionality.
  void Trace(Visitor* visitor) const override;

  void AddPendingRequest(std::unique_ptr<DocumentTransitionRequest>) override;

  VectorOf<std::unique_ptr<DocumentTransitionRequest>> TakePendingRequests();

 private:
  class FinishedResolved : public ScriptFunction::Callable {
   public:
    explicit FinishedResolved(DocumentTransitionSupplement* supplement,
                              DocumentTransition* transition,
                              Document*);
    ~FinishedResolved() override;

    ScriptValue Call(ScriptState*, ScriptValue) override;
    void Trace(Visitor* visitor) const override;

   private:
    Member<DocumentTransitionSupplement> supplement_;
    Member<DocumentTransition> transition_;
    Member<Document> document_;
  };

  void ResetTransition(DocumentTransition* transition);
  DocumentTransition* StartTransition(ScriptState* script_state,
                                      Document& document,
                                      V8DocumentTransitionCallback* callback,
                                      ExceptionState& exception_state);

  Member<DocumentTransition> transition_;

  VectorOf<std::unique_ptr<DocumentTransitionRequest>> pending_requests_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_SUPPLEMENT_H_
