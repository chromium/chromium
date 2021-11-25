// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ABORT_SIGNAL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ABORT_SIGNAL_H_

#include "base/callback_forward.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ExecutionContext;
class ScriptState;

// Implementation of https://dom.spec.whatwg.org/#interface-AbortSignal
class CORE_EXPORT AbortSignal : public EventTargetWithInlineData {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // The base class for "abort algorithm" defined at
  // https://dom.spec.whatwg.org/#abortsignal-abort-algorithms. This is
  // semantically equivalent to base::OnceClosure but is GarbageCollected.
  class Algorithm : public GarbageCollected<Algorithm> {
   public:
    virtual ~Algorithm() = default;

    // Called when the associated signal is aborted. This is called at most
    // once.
    virtual void Run() = 0;

    virtual void Trace(Visitor* visitor) const {}
  };

  explicit AbortSignal(ExecutionContext*);
  ~AbortSignal() override;

  // abort_signal.idl
  static AbortSignal* abort(ScriptState*);
  static AbortSignal* abort(ScriptState*, ScriptValue reason);
  ScriptValue reason(ScriptState*);
  bool aborted() const { return !abort_reason_.IsEmpty(); }
  DEFINE_ATTRIBUTE_EVENT_LISTENER(abort, kAbort)

  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // Internal API

  // The "add an algorithm" algorithm from the standard:
  // https://dom.spec.whatwg.org/#abortsignal-add for dependent features to call
  // to be notified when abort has been signalled.
  void AddAlgorithm(Algorithm* algorithm);

  // Same with above but with a base::OnceClosure. Use this only when you're
  // sure the objects attached to the callback don't form a reference cycle.
  void AddAlgorithm(base::OnceClosure algorithm);

  //
  // The "remove an algorithm" algorithm
  // https://dom.spec.whatwg.org/#abortsignal-remove is not yet implemented as
  // it has no users currently. See
  // https://docs.google.com/document/d/1OuoCG2uiijbAwbCw9jaS7tHEO0LBO_4gMNio1ox0qlY/edit#heading=h.m1zf7fypmlb9
  // for discussion.
  //

  // The "To signal abort" algorithm from the standard:
  // https://dom.spec.whatwg.org/#abortsignal-add. Run all algorithms that were
  // added by AddAlgorithm(), in order of addition, then fire an "abort"
  // event. Does nothing if called more than once.
  void SignalAbort(ScriptState*);
  void SignalAbort(ScriptState*, ScriptValue reason);

  // The "follow" algorithm from the standard:
  // https://dom.spec.whatwg.org/#abortsignal-follow
  // |this| is the followingSignal described in the standard.
  void Follow(ScriptState*, AbortSignal* parent);

  virtual bool IsTaskSignal() const { return false; }

  void Trace(Visitor*) const override;

 private:
  // https://dom.spec.whatwg.org/#abortsignal-abort-reason
  // There is one difference from the spec. The value is empty instead of
  // undefined when this signal is not aborted. This is because
  // ScriptValue::IsUndefined requires callers to enter a V8 context whereas
  // ScriptValue::IsEmpty does not.
  ScriptValue abort_reason_;
  HeapVector<Member<Algorithm>> abort_algorithms_;
  Member<ExecutionContext> execution_context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ABORT_SIGNAL_H_
