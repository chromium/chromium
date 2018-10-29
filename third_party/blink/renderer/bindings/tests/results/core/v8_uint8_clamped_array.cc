// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/interface.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/v8_uint8_clamped_array.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_configuration.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_shared_array_buffer.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
#include "third_party/blink/renderer/platform/wtf/get_ptr.h"

namespace blink {

// Suppress warning: global constructors, because struct WrapperTypeInfo is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
const WrapperTypeInfo V8Uint8ClampedArray::wrapperTypeInfo = {
    gin::kEmbedderBlink,
    nullptr,
    nullptr,
    "Uint8ClampedArray",
    &V8ArrayBufferView::wrapperTypeInfo,
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

// not [ActiveScriptWrappable]
static_assert(
    !std::is_base_of<ActiveScriptWrappableBase, TestUint8ClampedArray>::value,
    "TestUint8ClampedArray inherits from ActiveScriptWrappable<>, but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");
static_assert(
    std::is_same<decltype(&TestUint8ClampedArray::HasPendingActivity),
                 decltype(&ScriptWrappable::HasPendingActivity)>::value,
    "TestUint8ClampedArray is overriding hasPendingActivity(), but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");

TestUint8ClampedArray* V8Uint8ClampedArray::ToImpl(v8::Local<v8::Object> object) {
  DCHECK(object->IsUint8ClampedArray());
  ScriptWrappable* scriptWrappable = ToScriptWrappable(object);
  if (scriptWrappable)
    return scriptWrappable->ToImpl<TestUint8ClampedArray>();

  v8::Local<v8::Uint8ClampedArray> v8View = object.As<v8::Uint8ClampedArray>();
  v8::Local<v8::Object> arrayBuffer = v8View->Buffer();
  TestUint8ClampedArray* typedArray = nullptr;
  if (arrayBuffer->IsArrayBuffer()) {
    typedArray = TestUint8ClampedArray::Create(V8ArrayBuffer::ToImpl(arrayBuffer), v8View->ByteOffset(), v8View->Length());
  } else if (arrayBuffer->IsSharedArrayBuffer()) {
    typedArray = TestUint8ClampedArray::Create(V8SharedArrayBuffer::ToImpl(arrayBuffer), v8View->ByteOffset(), v8View->Length());
  } else {
    NOTREACHED();
  }
  v8::Local<v8::Object> associatedWrapper = typedArray->AssociateWithWrapper(v8::Isolate::GetCurrent(), typedArray->GetWrapperTypeInfo(), object);
  DCHECK(associatedWrapper == object);

  return typedArray->ToImpl<TestUint8ClampedArray>();
}

TestUint8ClampedArray* V8Uint8ClampedArray::ToImplWithTypeCheck(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return value->IsUint8ClampedArray() ? ToImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

TestUint8ClampedArray* NativeValueTraits<TestUint8ClampedArray>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  TestUint8ClampedArray* nativeValue = V8Uint8ClampedArray::ToImplWithTypeCheck(isolate, value);
  if (!nativeValue) {
    exceptionState.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
        "Uint8ClampedArray"));
  }
  return nativeValue;
}

}  // namespace blink
