// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/interface.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/v8_array_buffer_view.h"

#include <algorithm>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_big_int_64_array.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_big_uint_64_array.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_data_view.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_configuration.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_float32_array.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_float64_array.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_int16_array.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_int32_array.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_int8_array.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_shared_array_buffer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_uint16_array.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_uint32_array.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_uint8_array.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_uint8_clamped_array.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
#include "third_party/blink/renderer/platform/scheduler/public/cooperative_scheduling_manager.h"
#include "third_party/blink/renderer/platform/wtf/get_ptr.h"

namespace blink {

// not [ActiveScriptWrappable]
static_assert(
    !std::is_base_of<ActiveScriptWrappableBase, TestArrayBufferView>::value,
    "TestArrayBufferView inherits from ActiveScriptWrappable<>, but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");
static_assert(
    std::is_same<decltype(&TestArrayBufferView::HasPendingActivity),
                 decltype(&ScriptWrappable::HasPendingActivity)>::value,
    "TestArrayBufferView is overriding hasPendingActivity(), but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");

TestArrayBufferView* V8ArrayBufferView::ToImpl(v8::Local<v8::Object> object) {
  DCHECK(object->IsArrayBufferView());
  ScriptWrappable* script_wrappable = ToScriptWrappable(object);
  if (script_wrappable)
    return script_wrappable->ToImpl<TestArrayBufferView>();

  if (object->IsInt8Array())
    return V8Int8Array::ToImpl(object);
  if (object->IsInt16Array())
    return V8Int16Array::ToImpl(object);
  if (object->IsInt32Array())
    return V8Int32Array::ToImpl(object);
  if (object->IsUint8Array())
    return V8Uint8Array::ToImpl(object);
  if (object->IsUint8ClampedArray())
    return V8Uint8ClampedArray::ToImpl(object);
  if (object->IsUint16Array())
    return V8Uint16Array::ToImpl(object);
  if (object->IsUint32Array())
    return V8Uint32Array::ToImpl(object);
  if (object->IsBigInt64Array())
    return V8BigInt64Array::ToImpl(object);
  if (object->IsBigUint64Array())
    return V8BigUint64Array::ToImpl(object);
  if (object->IsFloat32Array())
    return V8Float32Array::ToImpl(object);
  if (object->IsFloat64Array())
    return V8Float64Array::ToImpl(object);
  if (object->IsDataView())
    return V8DataView::ToImpl(object);

  NOTREACHED();
  return nullptr;
}

TestArrayBufferView* V8ArrayBufferView::ToImplWithTypeCheck(
    v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return value->IsArrayBufferView() ? ToImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

}  // namespace blink
