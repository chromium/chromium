// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_SUBMIT_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_SUBMIT_EVENT_H_

#include <optional>

#include "base/functional/callback.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

class HTMLElement;
class SubmitEventInit;

class SubmitEvent : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static SubmitEvent* Create(const AtomicString& type,
                             const SubmitEventInit* event_init);
  SubmitEvent(const AtomicString& type, const SubmitEventInit* event_init);

  void Trace(Visitor* visitor) const override;
  HTMLElement* submitter() const { return submitter_.Get(); }
  const AtomicString& InterfaceName() const override;

  bool agentInvoked() const { return agent_invoked_; }
  void respondWith(ScriptState*, ScriptPromise<IDLAny>, ExceptionState&);

  // NOTE: The promise returned by this getter must be executed using the
  // script_state also returned by the getter, and not with any other
  // script_state. Doing so would be a cross-world security leak.
  class PromiseResult final {
    STACK_ALLOCATED();

   public:
    using Callback = base::OnceCallback<void(ScriptState*, ScriptValue)>;

    PromiseResult(ScriptState* script_state,
                  ScriptPromise<IDLAny> script_promise)
        : script_state_(script_state), promise_(script_promise) {}

    void Then(Callback on_fulfilled, Callback on_rejected) && {
      ScriptState::Scope scope(script_state_);
      promise_.Then(
          script_state_,
          MakeGarbageCollected<RespondWithHandler>(std::move(on_fulfilled)),
          MakeGarbageCollected<RespondWithHandler>(std::move(on_rejected)));
    }

   private:
    class RespondWithHandler : public ThenCallable<IDLAny, RespondWithHandler> {
     public:
      explicit RespondWithHandler(Callback callback)
          : callback_(std::move(callback)) {}
      void React(ScriptState* script_state, ScriptValue value) {
        std::move(callback_).Run(script_state, value);
      }
      void Trace(Visitor* visitor) const override {
        ThenCallable<IDLAny, RespondWithHandler>::Trace(visitor);
      }

     private:
      Callback callback_;
    };

    ScriptState* script_state_;
    ScriptPromise<IDLAny> promise_;
  };
  std::optional<PromiseResult> TakeRespondWithPromise();

 private:
  Member<HTMLElement> submitter_;
  MemberScriptPromise<IDLAny> respond_with_promise_;
  Member<ScriptState> respond_with_script_state_;
  bool agent_invoked_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_SUBMIT_EVENT_H_
