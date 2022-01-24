// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/observable_array.h"

#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/platform/bindings/dom_data_store.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-proxy.h"

namespace blink {

namespace bindings {

// static
const WrapperTypeInfo ObservableArrayExoticObjectImpl::wrapper_type_info_body_{
    gin::kEmbedderBlink,
    /*install_interface_template_func=*/nullptr,
    /*install_context_dependent_props_func=*/nullptr,
    "ObservableArrayExoticObject",
    /*parent_class=*/nullptr,
    WrapperTypeInfo::kWrapperTypeNoPrototype,
    // v8::Proxy (without an internal field) is used as a (pseudo) wrapper.
    WrapperTypeInfo::kNoInternalFieldClassId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
    WrapperTypeInfo::kIdlObservableArray,
};

// static
const WrapperTypeInfo& ObservableArrayExoticObjectImpl::wrapper_type_info_ =
    ObservableArrayExoticObjectImpl::wrapper_type_info_body_;

ObservableArrayExoticObjectImpl::ObservableArrayExoticObjectImpl(
    bindings::ObservableArrayBase* observable_array_backing_list_object)
    : ObservableArrayExoticObject(observable_array_backing_list_object) {}

v8::MaybeLocal<v8::Value> ObservableArrayExoticObjectImpl::Wrap(
    ScriptState* script_state) {
  DCHECK(!DOMDataStore::ContainsWrapper(this, script_state->GetIsolate()));

  v8::Local<v8::Value> target;
  if (!ToV8Traits<bindings::ObservableArrayBase>::ToV8(script_state,
                                                       GetBackingListObject())
           .ToLocal(&target)) {
    return {};
  }
  CHECK(target->IsObject());
  v8::Local<v8::Object> handler;
  if (!GetBackingListObject()
           ->GetProxyHandlerObject(script_state)
           .ToLocal(&handler)) {
    return {};
  }
  v8::Local<v8::Proxy> proxy;
  if (!v8::Proxy::New(script_state->GetContext(), target.As<v8::Object>(),
                      handler)
           .ToLocal(&proxy)) {
    return {};
  }
  v8::Local<v8::Object> wrapper = proxy.As<v8::Object>();

  // Register the proxy object as a (pseudo) wrapper object although the proxy
  // object does not have an internal field pointing to a Blink object.
  const bool is_new_entry = script_state->World().DomDataStore().Set(
      script_state->GetIsolate(), this, GetWrapperTypeInfo(), wrapper);
  CHECK(is_new_entry);

  return wrapper;
}

v8::Local<v8::Object> ObservableArrayExoticObjectImpl::AssociateWithWrapper(
    v8::Isolate* isolate,
    const WrapperTypeInfo* wrapper_type_info,
    v8::Local<v8::Object> wrapper) {
  // The proxy object does not have an internal field and cannot be associated
  // with a Blink object directly.
  NOTREACHED();
  return {};
}

}  // namespace bindings

}  // namespace blink
