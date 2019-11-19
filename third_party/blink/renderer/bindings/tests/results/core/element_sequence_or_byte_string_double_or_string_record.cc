// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/element_sequence_or_byte_string_double_or_string_record.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/double_or_string.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_element.h"
#include "third_party/blink/renderer/core/css/cssom/element_computed_style_map.h"
#include "third_party/blink/renderer/core/dom/child_node.h"
#include "third_party/blink/renderer/core/dom/non_document_type_child_node.h"
#include "third_party/blink/renderer/core/dom/parent_node.h"
#include "third_party/blink/renderer/core/fullscreen/element_fullscreen.h"

namespace blink {

ElementSequenceOrByteStringDoubleOrStringRecord::ElementSequenceOrByteStringDoubleOrStringRecord() : type_(SpecificType::kNone) {}

const HeapVector<std::pair<String, DoubleOrString>>& ElementSequenceOrByteStringDoubleOrStringRecord::GetAsByteStringDoubleOrStringRecord() const {
  DCHECK(IsByteStringDoubleOrStringRecord());
  return byte_string_double_or_string_record_;
}

void ElementSequenceOrByteStringDoubleOrStringRecord::SetByteStringDoubleOrStringRecord(const HeapVector<std::pair<String, DoubleOrString>>& value) {
  DCHECK(IsNull());
  byte_string_double_or_string_record_ = value;
  type_ = SpecificType::kByteStringDoubleOrStringRecord;
}

ElementSequenceOrByteStringDoubleOrStringRecord ElementSequenceOrByteStringDoubleOrStringRecord::FromByteStringDoubleOrStringRecord(const HeapVector<std::pair<String, DoubleOrString>>& value) {
  ElementSequenceOrByteStringDoubleOrStringRecord container;
  container.SetByteStringDoubleOrStringRecord(value);
  return container;
}

const HeapVector<Member<Element>>& ElementSequenceOrByteStringDoubleOrStringRecord::GetAsElementSequence() const {
  DCHECK(IsElementSequence());
  return element_sequence_;
}

void ElementSequenceOrByteStringDoubleOrStringRecord::SetElementSequence(const HeapVector<Member<Element>>& value) {
  DCHECK(IsNull());
  element_sequence_ = value;
  type_ = SpecificType::kElementSequence;
}

ElementSequenceOrByteStringDoubleOrStringRecord ElementSequenceOrByteStringDoubleOrStringRecord::FromElementSequence(const HeapVector<Member<Element>>& value) {
  ElementSequenceOrByteStringDoubleOrStringRecord container;
  container.SetElementSequence(value);
  return container;
}

ElementSequenceOrByteStringDoubleOrStringRecord::ElementSequenceOrByteStringDoubleOrStringRecord(const ElementSequenceOrByteStringDoubleOrStringRecord&) = default;
ElementSequenceOrByteStringDoubleOrStringRecord::~ElementSequenceOrByteStringDoubleOrStringRecord() = default;
ElementSequenceOrByteStringDoubleOrStringRecord& ElementSequenceOrByteStringDoubleOrStringRecord::operator=(const ElementSequenceOrByteStringDoubleOrStringRecord&) = default;

void ElementSequenceOrByteStringDoubleOrStringRecord::Trace(blink::Visitor* visitor) {
  visitor->Trace(byte_string_double_or_string_record_);
  visitor->Trace(element_sequence_);
}

void V8ElementSequenceOrByteStringDoubleOrStringRecord::ToImpl(
    v8::Isolate* isolate,
    v8::Local<v8::Value> v8_value,
    ElementSequenceOrByteStringDoubleOrStringRecord& impl,
    UnionTypeConversionMode conversion_mode,
    ExceptionState& exception_state) {
  if (v8_value.IsEmpty())
    return;

  if (conversion_mode == UnionTypeConversionMode::kNullable && IsUndefinedOrNull(v8_value))
    return;

  if (HasCallableIteratorSymbol(isolate, v8_value, exception_state)) {
    HeapVector<Member<Element>> cpp_value = NativeValueTraits<IDLSequence<Element>>::NativeValue(isolate, v8_value, exception_state);
    if (exception_state.HadException())
      return;
    impl.SetElementSequence(cpp_value);
    return;
  }

  if (v8_value->IsObject()) {
    HeapVector<std::pair<String, DoubleOrString>> cpp_value = NativeValueTraits<IDLRecord<IDLByteString, DoubleOrString>>::NativeValue(isolate, v8_value, exception_state);
    if (exception_state.HadException())
      return;
    impl.SetByteStringDoubleOrStringRecord(cpp_value);
    return;
  }

  exception_state.ThrowTypeError("The provided value is not of type '(sequence<Element> or record<ByteString, (double or DOMString)>)'");
}

v8::Local<v8::Value> ToV8(const ElementSequenceOrByteStringDoubleOrStringRecord& impl, v8::Local<v8::Object> creationContext, v8::Isolate* isolate) {
  switch (impl.type_) {
    case ElementSequenceOrByteStringDoubleOrStringRecord::SpecificType::kNone:
      return v8::Null(isolate);
    case ElementSequenceOrByteStringDoubleOrStringRecord::SpecificType::kByteStringDoubleOrStringRecord:
      return ToV8(impl.GetAsByteStringDoubleOrStringRecord(), creationContext, isolate);
    case ElementSequenceOrByteStringDoubleOrStringRecord::SpecificType::kElementSequence:
      return ToV8(impl.GetAsElementSequence(), creationContext, isolate);
    default:
      NOTREACHED();
  }
  return v8::Local<v8::Value>();
}

ElementSequenceOrByteStringDoubleOrStringRecord NativeValueTraits<ElementSequenceOrByteStringDoubleOrStringRecord>::NativeValue(
    v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exception_state) {
  ElementSequenceOrByteStringDoubleOrStringRecord impl;
  V8ElementSequenceOrByteStringDoubleOrStringRecord::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exception_state);
  return impl;
}

}  // namespace blink
