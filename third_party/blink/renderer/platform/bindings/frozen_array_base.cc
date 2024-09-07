// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/frozen_array_base.h"

#include "third_party/blink/renderer/platform/bindings/dom_data_store.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink::bindings {

namespace {

const WrapperTypeInfo frozen_array_wrapper_type_info_{
    gin::kEmbedderBlink,
    // JS objects for IDL frozen array types are implemented as JS Arrays,
    // which don't support V8 internal fields. Neither v8::FunctionTemplate nor
    // v8::ObjectTemplate is used.
    nullptr,  // install_interface_template_func
    nullptr,  // install_context_dependent_props_func
    "FrozenArray",
    nullptr,  // parent_class
    kDOMWrappersTag,
    kDOMWrappersTag,
    WrapperTypeInfo::kWrapperTypeNoPrototype,
    WrapperTypeInfo::kNoInternalFieldClassId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
    WrapperTypeInfo::kCustomWrappableKind,
};

}  // namespace

// We don't use the bindings code generator for IDL FrozenArray, so we define
// FrozenArrayBase::wrapper_type_info_ manually here.
const WrapperTypeInfo& FrozenArrayBase::wrapper_type_info_ =
    frozen_array_wrapper_type_info_;

v8::Local<v8::Value> FrozenArrayBase::ToV8(ScriptState* script_state) const {
  return const_cast<FrozenArrayBase*>(this)->ToV8(script_state);
}

v8::Local<v8::Value> FrozenArrayBase::ToV8(ScriptState* script_state) {
  v8::Local<v8::Object> wrapper;
  if (DOMDataStore::GetWrapper(script_state, this).ToLocal(&wrapper))
      [[likely]] {
    return wrapper;
  }

  return Wrap(script_state);
}

v8::Local<v8::Value> FrozenArrayBase::Wrap(ScriptState* script_state) {
  DCHECK(!DOMDataStore::ContainsWrapper(script_state->GetIsolate(), this));

  v8::Local<v8::Value> wrapper = MakeV8ArrayToBeFrozen(script_state);

  wrapper.As<v8::Object>()->SetIntegrityLevel(script_state->GetContext(),
                                              v8::IntegrityLevel::kFrozen);

  return AssociateWithWrapper(script_state->GetIsolate(), GetWrapperTypeInfo(),
                              wrapper.As<v8::Object>());
}

v8::Local<v8::Object> FrozenArrayBase::AssociateWithWrapper(
    v8::Isolate* isolate,
    const WrapperTypeInfo* wrapper_type_info,
    v8::Local<v8::Object> wrapper) {
  // Since v8::Array doesn't have an internal field, just set the wrapper to
  // the DOMDataStore and never call V8DOMWrapper::SetNativeInfo unlike regular
  // ScriptWrappables.
  CHECK(DOMDataStore::SetWrapper(isolate, this, GetWrapperTypeInfo(), wrapper));
  return wrapper;
}

}  // namespace blink::bindings
