// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/long_sequence_or_event.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_event.h"

namespace blink {

LongSequenceOrEvent::LongSequenceOrEvent() : type_(SpecificType::kNone) {}

Event* LongSequenceOrEvent::GetAsEvent() const {
  DCHECK(IsEvent());
  return event_;
}

void LongSequenceOrEvent::SetEvent(Event* value) {
  DCHECK(IsNull());
  event_ = value;
  type_ = SpecificType::kEvent;
}

LongSequenceOrEvent LongSequenceOrEvent::FromEvent(Event* value) {
  LongSequenceOrEvent container;
  container.SetEvent(value);
  return container;
}

const Vector<int32_t>& LongSequenceOrEvent::GetAsLongSequence() const {
  DCHECK(IsLongSequence());
  return long_sequence_;
}

void LongSequenceOrEvent::SetLongSequence(const Vector<int32_t>& value) {
  DCHECK(IsNull());
  long_sequence_ = value;
  type_ = SpecificType::kLongSequence;
}

LongSequenceOrEvent LongSequenceOrEvent::FromLongSequence(const Vector<int32_t>& value) {
  LongSequenceOrEvent container;
  container.SetLongSequence(value);
  return container;
}

LongSequenceOrEvent::LongSequenceOrEvent(const LongSequenceOrEvent&) = default;
LongSequenceOrEvent::~LongSequenceOrEvent() = default;
LongSequenceOrEvent& LongSequenceOrEvent::operator=(const LongSequenceOrEvent&) = default;

void LongSequenceOrEvent::Trace(blink::Visitor* visitor) {
  visitor->Trace(event_);
}

void V8LongSequenceOrEvent::ToImpl(v8::Isolate* isolate, v8::Local<v8::Value> v8Value, LongSequenceOrEvent& impl, UnionTypeConversionMode conversionMode, ExceptionState& exceptionState) {
  if (v8Value.IsEmpty())
    return;

  if (conversionMode == UnionTypeConversionMode::kNullable && IsUndefinedOrNull(v8Value))
    return;

  if (V8Event::hasInstance(v8Value, isolate)) {
    Event* cppValue = V8Event::ToImpl(v8::Local<v8::Object>::Cast(v8Value));
    impl.SetEvent(cppValue);
    return;
  }

  if (HasCallableIteratorSymbol(isolate, v8Value, exceptionState)) {
    Vector<int32_t> cppValue = NativeValueTraits<IDLSequence<IDLLong>>::NativeValue(isolate, v8Value, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.SetLongSequence(cppValue);
    return;
  }

  exceptionState.ThrowTypeError("The provided value is not of type '(sequence<long> or Event)'");
}

v8::Local<v8::Value> ToV8(const LongSequenceOrEvent& impl, v8::Local<v8::Object> creationContext, v8::Isolate* isolate) {
  switch (impl.type_) {
    case LongSequenceOrEvent::SpecificType::kNone:
      return v8::Null(isolate);
    case LongSequenceOrEvent::SpecificType::kEvent:
      return ToV8(impl.GetAsEvent(), creationContext, isolate);
    case LongSequenceOrEvent::SpecificType::kLongSequence:
      return ToV8(impl.GetAsLongSequence(), creationContext, isolate);
    default:
      NOTREACHED();
  }
  return v8::Local<v8::Value>();
}

LongSequenceOrEvent NativeValueTraits<LongSequenceOrEvent>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  LongSequenceOrEvent impl;
  V8LongSequenceOrEvent::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exceptionState);
  return impl;
}

}  // namespace blink
