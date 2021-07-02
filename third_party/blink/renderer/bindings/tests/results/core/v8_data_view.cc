// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/interface.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/v8_data_view.h"

#include <algorithm>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_configuration.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_shared_array_buffer.h"
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
    !std::is_base_of<ActiveScriptWrappableBase, TestDataView>::value,
    "TestDataView inherits from ActiveScriptWrappable<>, but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");
static_assert(
    std::is_same<decltype(&TestDataView::HasPendingActivity),
                 decltype(&ScriptWrappable::HasPendingActivity)>::value,
    "TestDataView is overriding hasPendingActivity(), but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");

TestDataView* V8DataView::ToImpl(v8::Local<v8::Object> object) {
  DCHECK(object->IsDataView());
  ScriptWrappable* script_wrappable = ToScriptWrappable(object);
  if (script_wrappable)
    return script_wrappable->ToImpl<TestDataView>();

  v8::Local<v8::DataView> v8_view = object.As<v8::DataView>();
  v8::Local<v8::Object> array_buffer = v8_view->Buffer();
  TestDataView* typed_array = nullptr;
  if (array_buffer->IsArrayBuffer()) {
    typed_array = TestDataView::Create(
        V8ArrayBuffer::ToImpl(array_buffer),
        v8_view->ByteOffset(),
        v8_view->ByteLength());
  } else if (array_buffer->IsSharedArrayBuffer()) {
    typed_array = TestDataView::Create(
        V8SharedArrayBuffer::ToImpl(array_buffer),
        v8_view->ByteOffset(),
        v8_view->ByteLength());
  } else {
    NOTREACHED();
  }
  v8::Local<v8::Object> associated_wrapper =
        typed_array->AssociateWithWrapper(
            v8::Isolate::GetCurrent(), typed_array->GetWrapperTypeInfo(), object);
  DCHECK(associated_wrapper == object);

  return typed_array->ToImpl<TestDataView>();
}

TestDataView* V8DataView::ToImplWithTypeCheck(
    v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return value->IsDataView() ? ToImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

}  // namespace blink
