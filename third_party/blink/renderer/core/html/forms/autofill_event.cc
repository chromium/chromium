// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/autofill_event.h"

#include "base/functional/bind.h"
#include "third_party/blink/public/web/web_autofill_client.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

AutofillEvent::AutofillEvent(
    const AtomicString& type,
    HeapVector<std::pair<Member<Element>, String>> field_data,
    const base::UnguessableToken& fill_id,
    bool supports_refill)
    : field_data_(field_data,
                  [](const std::pair<Member<Element>, String>& pair) {
                    AutofillFieldData* data = AutofillFieldData::Create();
                    data->setField(pair.first.Get());
                    data->setValue(pair.second);
                    return data;
                  }),
      fill_id_(fill_id),
      supports_refill_(supports_refill) {}

AutofillEvent* AutofillEvent::Create(
    const AtomicString& type,
    HeapVector<std::pair<Member<Element>, String>> field_data,
    const base::UnguessableToken& fill_id,
    bool supports_refill) {
  AutofillEvent* event = MakeGarbageCollected<AutofillEvent>(
      type, std::move(field_data), fill_id, supports_refill);
  event->initEvent(type, false, false);
  return event;
}

void AutofillEvent::Trace(Visitor* visitor) const {
  visitor->Trace(field_data_);
  visitor->Trace(refill_callback_);
  Event::Trace(visitor);
}

const AtomicString& AutofillEvent::InterfaceName() const {
  return event_interface_names::kAutofillEvent;
}

const HeapVector<Member<AutofillFieldData>>& AutofillEvent::autofillValues()
    const {
  return field_data_;
}

// Helper class to create a JavaScript function that calls DoRefill and returns
// a Promise that resolves when the refill completes.
class AutofillRefillFunction : public ScriptFunction {
 public:
  explicit AutofillRefillFunction(AutofillEvent* event) : event_(event) {}

  ScriptValue Call(ScriptState* script_state, ScriptValue) override {
    auto* resolver =
        MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
    ScriptPromise<IDLUndefined> promise = resolver->Promise();

    event_->DoRefill(resolver);

    return ScriptValue(script_state->GetIsolate(),
                       promise.V8Promise().template As<v8::Value>());
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(event_);
    ScriptFunction::Trace(visitor);
  }

 private:
  Member<AutofillEvent> event_;
};

V8AutofillRefillCallback* AutofillEvent::refill(ScriptState* script_state) {
  if (!supports_refill_) {
    // This manifests in JS as `e.refill === null`.
    return nullptr;
  }

  // Lazily create the callback on first access.
  if (!refill_callback_) {
    auto* function = MakeGarbageCollected<AutofillRefillFunction>(this);
    refill_callback_ = V8AutofillRefillCallback::Create(
        function->ToV8Function(script_state).template As<v8::Object>());
  }

  return refill_callback_.Get();
}

void AutofillEvent::DoRefill(ScriptPromiseResolver<IDLUndefined>* resolver) {
  // The Document is the event target.
  EventTarget* event_target = target();
  if (!event_target) {
    resolver->Reject();
    return;
  }
  Document* document = DynamicTo<Document>(event_target->ToNode());
  if (!document) {
    resolver->Reject();
    return;
  }
  LocalFrame* frame = document->GetFrame();
  if (!frame) {
    resolver->Reject();
    return;
  }
  WebAutofillClient* fill_client = frame->GetAutofillClient();
  if (!fill_client) {
    resolver->Reject();
    return;
  }

  // Pass the fill_id and a callback that resolves/rejects the promise.
  fill_client->RequestRefill(
      fill_id_, blink::BindOnce(
                    [](Persistent<ScriptPromiseResolver<IDLUndefined>> resolver,
                       bool success) {
                      if (success) {
                        resolver->Resolve();
                      } else {
                        resolver->Reject();
                      }
                    },
                    WrapPersistent(resolver)));
}

}  // namespace blink
