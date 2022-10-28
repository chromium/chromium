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
      public DocumentTransition::Delegate {
 public:
  static const char kSupplementName[];

  // Supplement functionality.
  static DocumentTransitionSupplement* From(Document&);
  static DocumentTransitionSupplement* FromIfExists(const Document&);

  // Creates and starts a same-document DocumentTransition initiated using the
  // script API.
  static DocumentTransition* startViewTransition(
      ScriptState*,
      Document&,
      V8DocumentTransitionCallback* callback,
      ExceptionState&);

  // Creates a DocumentTransition to cache the state of a Document before a
  // navigation. The cached state is provided to the caller using the
  // |ViewTransitionStateCallback|.
  static void SnapshotDocumentForNavigation(
      Document&,
      DocumentTransition::ViewTransitionStateCallback);

  // Creates a DocumentTransition using cached state from the previous Document
  // of a navigation. This DocumentTransition is responsible for running
  // animations on the new Document using the cached state.
  static void CreateFromSnapshotForNavigation(
      Document&,
      ViewTransitionState transition_state);

  DocumentTransition* GetActiveTransition();

  explicit DocumentTransitionSupplement(Document&);
  ~DocumentTransitionSupplement() override;

  // GC functionality.
  void Trace(Visitor* visitor) const override;

  // DocumentTransition::Delegate implementation.
  void AddPendingRequest(std::unique_ptr<DocumentTransitionRequest>) override;
  VectorOf<std::unique_ptr<DocumentTransitionRequest>> TakePendingRequests();
  void OnTransitionFinished(DocumentTransition* transition) override;

 private:
  DocumentTransition* StartTransition(ScriptState* script_state,
                                      Document& document,
                                      V8DocumentTransitionCallback* callback,
                                      ExceptionState& exception_state);
  void StartTransition(
      Document& document,
      DocumentTransition::ViewTransitionStateCallback callback);
  void StartTransition(Document& document,
                       ViewTransitionState transition_state);

  Member<DocumentTransition> transition_;

  VectorOf<std::unique_ptr<DocumentTransitionRequest>> pending_requests_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_SUPPLEMENT_H_
