// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ABORT_SIGNAL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ABORT_SIGNAL_H_

#include "base/callback_forward.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ExecutionContext;

// Implementation of https://dom.spec.whatwg.org/#interface-AbortSignal
class CORE_EXPORT AbortSignal final : public EventTargetWithInlineData {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit AbortSignal(ExecutionContext*);
  ~AbortSignal() override;

  // abort_signal.idl
  bool aborted() const { return aborted_flag_; }
  DEFINE_ATTRIBUTE_EVENT_LISTENER(abort);

  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // Internal API

  // The "add an algorithm" algorithm from the standard:
  // https://dom.spec.whatwg.org/#abortsignal-add for dependent features to call
  // to be notified when abort has been signalled. Callers should pass a
  // OnceClosure holding a weak pointer, unless the object needs to receive a
  // cancellation signal even after it otherwise would have been destroyed.
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
  void SignalAbort();

  // The "follow" algorithm from the standard:
  // https://dom.spec.whatwg.org/#abortsignal-follow
  // |this| is the followingSignal described in the standard.
  void Follow(AbortSignal* parentSignal);

  void Trace(Visitor*) override;

 private:
  bool aborted_flag_ = false;
  Vector<base::OnceClosure> abort_algorithms_;
  Member<ExecutionContext> execution_context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ABORT_SIGNAL_H_
