// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_H_

#include "cc/document_transition/document_transition_request.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class Document;
class DocumentTransitionInit;
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
  ScriptPromise prepare(ScriptState*, const DocumentTransitionInit*);
  void start();

  // This uses std::move semantics to take the request from this object.
  std::unique_ptr<Request> TakePendingRequest();

 private:
  friend class DocumentTransitionTest;

  enum class State { kIdle, kPreparing, kPrepared, kStarted };

  void NotifyHasChangesToCommit();

  void NotifyPrepareCommitted(uint32_t sequence_id);
  void NotifyStartCommitted();

  void ParseAndSetTransitionParameters(const DocumentTransitionInit* params);

  Member<Document> document_;

  State state_ = State::kIdle;

  Member<ScriptPromiseResolver> prepare_promise_resolver_;
  base::TimeDelta duration_;
  Request::Effect effect_ = Request::Effect::kNone;

  std::unique_ptr<Request> pending_request_;

  uint32_t prepare_sequence_id_ = 0u;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_H_
