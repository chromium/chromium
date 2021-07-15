// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_H_

#include <memory>

#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/graphics/document_transition_shared_element_id.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace cc {
class DocumentTransitionRequest;
}

namespace blink {

class AbortSignal;
class Document;
class DocumentTransitionPrepareOptions;
class DocumentTransitionStartOptions;
class Element;
class ExceptionState;
class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;

class CORE_EXPORT DocumentTransition
    : public ScriptWrappable,
      public ActiveScriptWrappable<DocumentTransition>,
      public ExecutionContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  using Request = cc::DocumentTransitionRequest;

  explicit DocumentTransition(Document*);

  // GC functionality.
  void Trace(Visitor* visitor) const override;

  // ExecutionContextLifecycleObserver implementation.
  void ContextDestroyed() override;

  // ActiveScriptWrappable functionality.
  bool HasPendingActivity() const override;

  // JavaScript API implementation.
  ScriptPromise prepare(ScriptState*,
                        const DocumentTransitionPrepareOptions*,
                        ExceptionState&);
  ScriptPromise start(ScriptState*,
                      const DocumentTransitionStartOptions*,
                      ExceptionState&);

  // This uses std::move semantics to take the request from this object.
  std::unique_ptr<Request> TakePendingRequest();

  // Returns true if the given element is active in this transition.
  bool IsActiveElement(const Element*) const;

  // Returns an identifier for the given shared element. Note that the element
  // must be active (i.e. `IsActive(element)` must be true).
  DocumentTransitionSharedElementId GetSharedElementId(const Element*) const;

  // We require shared elements to be contained. This check verifies that and
  // removes it from the shared list if it isn't. See
  // https://github.com/vmpstr/shared-element-transitions/issues/17
  void VerifySharedElements();

 private:
  friend class DocumentTransitionTest;

  enum class State { kIdle, kPreparing, kPrepared, kStarted };

  void NotifyHasChangesToCommit();

  void NotifyPrepareFinished(uint32_t sequence_id);
  void NotifyStartFinished(uint32_t sequence_id);

  // Sets new active shared elements. Note that this is responsible for making
  // sure we invalidate the right bits both on the old and new elements.
  void SetActiveSharedElements(HeapVector<Member<Element>> elements);
  void InvalidateActiveElements();

  // Used to defer visual updates between transition prepare finishing and
  // transition start to allow the page to set up the final scene
  // asynchronously.
  void StartDeferringCommits();
  void StopDeferringCommits();

  // Allow canceling a transition until it reaches start().
  void CancelPendingTransition(const char* abort_message);

  void Abort(AbortSignal* signal);

  Member<Document> document_;

  State state_ = State::kIdle;

  Member<ScriptPromiseResolver> prepare_promise_resolver_;
  Member<ScriptPromiseResolver> start_promise_resolver_;
  Member<AbortSignal> signal_;

  // `active_shared_elements_` represents elements that are identified as shared
  // during the current step of the transition. Specifically, it represents
  // `prepare()` call sharedElements if the state is kPreparing and `start()`
  // call sharedElements if the state is kStarted.
  // `prepare_shared_element_count_` represents the number of shared elements
  // that were specified in the `prepare()` call. This is used to verify that
  // the number of shared elements specified in the `prepare()` and `start()`
  // calls is the same.
  HeapVector<Member<Element>> active_shared_elements_;
  wtf_size_t prepare_shared_element_count_ = 0u;

  std::unique_ptr<Request> pending_request_;

  uint32_t last_prepare_sequence_id_ = 0u;
  uint32_t last_start_sequence_id_ = 0u;

  // Use a common counter for sequence ids to avoid confusion. Prepare
  // and start sequence ids are technically in a different namespace, but this
  // avoids any confusion while debugging.
  uint32_t next_sequence_id_ = 1u;

  // The document tag identifies the document to which this transition belongs.
  // It's unique among other local documents.
  uint32_t document_tag_ = 0u;

  bool deferring_commits_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_H_
