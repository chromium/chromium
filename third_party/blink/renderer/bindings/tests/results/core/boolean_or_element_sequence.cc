// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/boolean_or_element_sequence.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_element.h"
#include "third_party/blink/renderer/core/animation/element_animation.h"
#include "third_party/blink/renderer/core/css/cssom/element_computed_style_map.h"
#include "third_party/blink/renderer/core/dom/child_node.h"
#include "third_party/blink/renderer/core/dom/non_document_type_child_node.h"
#include "third_party/blink/renderer/core/dom/parent_node.h"
#include "third_party/blink/renderer/core/fullscreen/element_fullscreen.h"

namespace blink {

BooleanOrElementSequence::BooleanOrElementSequence() : type_(SpecificType::kNone) {}

bool BooleanOrElementSequence::GetAsBoolean() const {
  DCHECK(IsBoolean());
  return boolean_;
}

void BooleanOrElementSequence::SetBoolean(bool value) {
  DCHECK(IsNull());
  boolean_ = value;
  type_ = SpecificType::kBoolean;
}

BooleanOrElementSequence BooleanOrElementSequence::FromBoolean(bool value) {
  BooleanOrElementSequence container;
  container.SetBoolean(value);
  return container;
}

const HeapVector<Member<Element>>& BooleanOrElementSequence::GetAsElementSequence() const {
  DCHECK(IsElementSequence());
  return element_sequence_;
}

void BooleanOrElementSequence::SetElementSequence(const HeapVector<Member<Element>>& value) {
  DCHECK(IsNull());
  element_sequence_ = value;
  type_ = SpecificType::kElementSequence;
}

BooleanOrElementSequence BooleanOrElementSequence::FromElementSequence(const HeapVector<Member<Element>>& value) {
  BooleanOrElementSequence container;
  container.SetElementSequence(value);
  return container;
}

BooleanOrElementSequence::BooleanOrElementSequence(const BooleanOrElementSequence&) = default;
BooleanOrElementSequence::~BooleanOrElementSequence() = default;
BooleanOrElementSequence& BooleanOrElementSequence::operator=(const BooleanOrElementSequence&) = default;

void BooleanOrElementSequence::Trace(blink::Visitor* visitor) {
  visitor->Trace(element_sequence_);
}

void V8BooleanOrElementSequence::ToImpl(v8::Isolate* isolate, v8::Local<v8::Value> v8Value, BooleanOrElementSequence& impl, UnionTypeConversionMode conversionMode, ExceptionState& exceptionState) {
  if (v8Value.IsEmpty())
    return;

  if (conversionMode == UnionTypeConversionMode::kNullable && IsUndefinedOrNull(v8Value))
    return;

  if (HasCallableIteratorSymbol(isolate, v8Value, exceptionState)) {
    HeapVector<Member<Element>> cppValue = NativeValueTraits<IDLSequence<Element>>::NativeValue(isolate, v8Value, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.SetElementSequence(cppValue);
    return;
  }

  if (v8Value->IsBoolean()) {
    impl.SetBoolean(v8Value.As<v8::Boolean>()->Value());
    return;
  }

  {
    impl.SetBoolean(v8Value->BooleanValue(isolate->GetCurrentContext()).ToChecked());
    return;
  }
}

v8::Local<v8::Value> ToV8(const BooleanOrElementSequence& impl, v8::Local<v8::Object> creationContext, v8::Isolate* isolate) {
  switch (impl.type_) {
    case BooleanOrElementSequence::SpecificType::kNone:
      return v8::Null(isolate);
    case BooleanOrElementSequence::SpecificType::kBoolean:
      return v8::Boolean::New(isolate, impl.GetAsBoolean());
    case BooleanOrElementSequence::SpecificType::kElementSequence:
      return ToV8(impl.GetAsElementSequence(), creationContext, isolate);
    default:
      NOTREACHED();
  }
  return v8::Local<v8::Value>();
}

BooleanOrElementSequence NativeValueTraits<BooleanOrElementSequence>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  BooleanOrElementSequence impl;
  V8BooleanOrElementSequence::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exceptionState);
  return impl;
}

}  // namespace blink
