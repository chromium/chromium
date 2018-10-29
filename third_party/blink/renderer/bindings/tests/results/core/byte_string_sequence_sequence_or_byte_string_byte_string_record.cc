// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/byte_string_sequence_sequence_or_byte_string_byte_string_record.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"

namespace blink {

ByteStringSequenceSequenceOrByteStringByteStringRecord::ByteStringSequenceSequenceOrByteStringByteStringRecord() : type_(SpecificType::kNone) {}

const Vector<std::pair<String, String>>& ByteStringSequenceSequenceOrByteStringByteStringRecord::GetAsByteStringByteStringRecord() const {
  DCHECK(IsByteStringByteStringRecord());
  return byte_string_byte_string_record_;
}

void ByteStringSequenceSequenceOrByteStringByteStringRecord::SetByteStringByteStringRecord(const Vector<std::pair<String, String>>& value) {
  DCHECK(IsNull());
  byte_string_byte_string_record_ = value;
  type_ = SpecificType::kByteStringByteStringRecord;
}

ByteStringSequenceSequenceOrByteStringByteStringRecord ByteStringSequenceSequenceOrByteStringByteStringRecord::FromByteStringByteStringRecord(const Vector<std::pair<String, String>>& value) {
  ByteStringSequenceSequenceOrByteStringByteStringRecord container;
  container.SetByteStringByteStringRecord(value);
  return container;
}

const Vector<Vector<String>>& ByteStringSequenceSequenceOrByteStringByteStringRecord::GetAsByteStringSequenceSequence() const {
  DCHECK(IsByteStringSequenceSequence());
  return byte_string_sequence_sequence_;
}

void ByteStringSequenceSequenceOrByteStringByteStringRecord::SetByteStringSequenceSequence(const Vector<Vector<String>>& value) {
  DCHECK(IsNull());
  byte_string_sequence_sequence_ = value;
  type_ = SpecificType::kByteStringSequenceSequence;
}

ByteStringSequenceSequenceOrByteStringByteStringRecord ByteStringSequenceSequenceOrByteStringByteStringRecord::FromByteStringSequenceSequence(const Vector<Vector<String>>& value) {
  ByteStringSequenceSequenceOrByteStringByteStringRecord container;
  container.SetByteStringSequenceSequence(value);
  return container;
}

ByteStringSequenceSequenceOrByteStringByteStringRecord::ByteStringSequenceSequenceOrByteStringByteStringRecord(const ByteStringSequenceSequenceOrByteStringByteStringRecord&) = default;
ByteStringSequenceSequenceOrByteStringByteStringRecord::~ByteStringSequenceSequenceOrByteStringByteStringRecord() = default;
ByteStringSequenceSequenceOrByteStringByteStringRecord& ByteStringSequenceSequenceOrByteStringByteStringRecord::operator=(const ByteStringSequenceSequenceOrByteStringByteStringRecord&) = default;

void ByteStringSequenceSequenceOrByteStringByteStringRecord::Trace(blink::Visitor* visitor) {
}

void V8ByteStringSequenceSequenceOrByteStringByteStringRecord::ToImpl(v8::Isolate* isolate, v8::Local<v8::Value> v8Value, ByteStringSequenceSequenceOrByteStringByteStringRecord& impl, UnionTypeConversionMode conversionMode, ExceptionState& exceptionState) {
  if (v8Value.IsEmpty())
    return;

  if (conversionMode == UnionTypeConversionMode::kNullable && IsUndefinedOrNull(v8Value))
    return;

  if (HasCallableIteratorSymbol(isolate, v8Value, exceptionState)) {
    Vector<Vector<String>> cppValue = NativeValueTraits<IDLSequence<IDLSequence<IDLByteString>>>::NativeValue(isolate, v8Value, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.SetByteStringSequenceSequence(cppValue);
    return;
  }

  if (v8Value->IsObject()) {
    Vector<std::pair<String, String>> cppValue = NativeValueTraits<IDLRecord<IDLByteString, IDLByteString>>::NativeValue(isolate, v8Value, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.SetByteStringByteStringRecord(cppValue);
    return;
  }

  exceptionState.ThrowTypeError("The provided value is not of type '(sequence<sequence<ByteString>> or record<ByteString, ByteString>)'");
}

v8::Local<v8::Value> ToV8(const ByteStringSequenceSequenceOrByteStringByteStringRecord& impl, v8::Local<v8::Object> creationContext, v8::Isolate* isolate) {
  switch (impl.type_) {
    case ByteStringSequenceSequenceOrByteStringByteStringRecord::SpecificType::kNone:
      return v8::Null(isolate);
    case ByteStringSequenceSequenceOrByteStringByteStringRecord::SpecificType::kByteStringByteStringRecord:
      return ToV8(impl.GetAsByteStringByteStringRecord(), creationContext, isolate);
    case ByteStringSequenceSequenceOrByteStringByteStringRecord::SpecificType::kByteStringSequenceSequence:
      return ToV8(impl.GetAsByteStringSequenceSequence(), creationContext, isolate);
    default:
      NOTREACHED();
  }
  return v8::Local<v8::Value>();
}

ByteStringSequenceSequenceOrByteStringByteStringRecord NativeValueTraits<ByteStringSequenceSequenceOrByteStringByteStringRecord>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  ByteStringSequenceSequenceOrByteStringByteStringRecord impl;
  V8ByteStringSequenceSequenceOrByteStringByteStringRecord::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exceptionState);
  return impl;
}

}  // namespace blink
