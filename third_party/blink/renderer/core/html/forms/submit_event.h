// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_SUBMIT_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_SUBMIT_EVENT_H_

#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
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
  // script_state. Doing so would be a cross-world security leak. E.g.,
  //   auto val = submit_event->RespondWithPromise();
  //   ScriptState::Scope scope(val->second);
  //   val->first.Unwrap()...
  std::optional<std::pair<MemberScriptPromise<IDLAny>, Member<ScriptState>>>
  RespondWithPromise() const;

 private:
  Member<HTMLElement> submitter_;
  std::pair<MemberScriptPromise<IDLAny>, Member<ScriptState>>
      respond_with_promise_;
  bool agent_invoked_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_SUBMIT_EVENT_H_
