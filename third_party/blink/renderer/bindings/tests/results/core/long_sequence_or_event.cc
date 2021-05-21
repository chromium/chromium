// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/long_sequence_or_event.h"

#include "base/cxx17_backports.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_iterator.h"
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

void LongSequenceOrEvent::Trace(Visitor* visitor) const {
  visitor->Trace(event_);
}

void V8LongSequenceOrEvent::ToImpl(
    v8::Isolate* isolate,
    v8::Local<v8::Value> v8_value,
    LongSequenceOrEvent& impl,
    UnionTypeConversionMode conversion_mode,
    ExceptionState& exception_state) {
  if (v8_value.IsEmpty())
    return;

  if (conversion_mode == UnionTypeConversionMode::kNullable && IsUndefinedOrNull(v8_value))
    return;

  if (V8Event::HasInstance(v8_value, isolate)) {
    Event* cpp_value = V8Event::ToImpl(v8::Local<v8::Object>::Cast(v8_value));
    impl.SetEvent(cpp_value);
    return;
  }

  if (v8_value->IsObject()) {
    ScriptIterator script_iterator = ScriptIterator::FromIterable(
        isolate, v8_value.As<v8::Object>(), exception_state);
    if (exception_state.HadException())
      return;
    if (!script_iterator.IsNull()) {
      Vector<int32_t> cpp_value{ NativeValueTraits<IDLSequence<IDLLong>>::NativeValue(isolate, std::move(script_iterator), exception_state) };
      if (exception_state.HadException())
        return;
      impl.SetLongSequence(cpp_value);
      return;
    }
  }

  exception_state.ThrowTypeError("The provided value is not of type '(sequence<long> or Event)'");
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

LongSequenceOrEvent NativeValueTraits<LongSequenceOrEvent>::NativeValue(
    v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exception_state) {
  LongSequenceOrEvent impl;
  V8LongSequenceOrEvent::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exception_state);
  return impl;
}

}  // namespace blink

