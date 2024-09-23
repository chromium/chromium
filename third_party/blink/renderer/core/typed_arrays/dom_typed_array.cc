// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"

#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/platform/bindings/dom_data_store.h"

namespace blink {

template <typename T, typename V8TypedArray, bool clamped>
v8::Local<v8::Value> DOMTypedArray<T, V8TypedArray, clamped>::Wrap(
    ScriptState* script_state) {
  DCHECK(!DOMDataStore::ContainsWrapper(script_state->GetIsolate(), this));

  const WrapperTypeInfo* wrapper_type_info = GetWrapperTypeInfo();
  DOMArrayBufferBase* buffer = BufferBase();
  v8::Local<v8::Value> v8_buffer =
      ToV8Traits<DOMArrayBufferBase>::ToV8(script_state, buffer);
  DCHECK_EQ(IsShared(), v8_buffer->IsSharedArrayBuffer());

  v8::Local<v8::Object> wrapper;
  {
    v8::Context::Scope context_scope(script_state->GetContext());
    if (IsShared()) {
      wrapper = V8TypedArray::New(v8_buffer.As<v8::SharedArrayBuffer>(),
                                  byteOffset(), length());
    } else {
      wrapper = V8TypedArray::New(v8_buffer.As<v8::ArrayBuffer>(), byteOffset(),
                                  length());
    }
  }

  return AssociateWithWrapper(script_state->GetIsolate(), wrapper_type_info,
                              wrapper);
}

#define DOMTYPEDARRAY_FOREACH_VIEW_TYPE(V) \
  V(int8_t, Int8, false)                   \
  V(int16_t, Int16, false)                 \
  V(int32_t, Int32, false)                 \
  V(uint8_t, Uint8, false)                 \
  V(uint8_t, Uint8Clamped, true)           \
  V(uint16_t, Uint16, false)               \
  V(uint32_t, Uint32, false)               \
  V(uint16_t, Float16, false)              \
  V(float, Float32, false)                 \
  V(double, Float64, false)                \
  V(int64_t, BigInt64, false)              \
  V(uint64_t, BigUint64, false)

#define DOMTYPEDARRAY_DEFINE_WRAPPERTYPEINFO(val_t, Type, clamped)             \
  template <>                                                                  \
  const WrapperTypeInfo                                                        \
      DOMTypedArray<val_t, v8::Type##Array, clamped>::wrapper_type_info_body_{ \
          gin::kEmbedderBlink,                                                 \
          nullptr,                                                             \
          nullptr,                                                             \
          #Type "Array",                                                       \
          nullptr,                                                             \
          kDOMWrappersTag,                                                     \
          kDOMWrappersTag,                                                     \
          WrapperTypeInfo::kWrapperTypeObjectPrototype,                        \
          WrapperTypeInfo::kObjectClassId,                                     \
          WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,               \
          WrapperTypeInfo::kIdlBufferSourceType,                               \
      };                                                                       \
  template <>                                                                  \
  const WrapperTypeInfo& DOMTypedArray<val_t, v8::Type##Array,                 \
                                       clamped>::wrapper_type_info_ =          \
      DOMTypedArray<val_t, v8::Type##Array, clamped>::wrapper_type_info_body_;
DOMTYPEDARRAY_FOREACH_VIEW_TYPE(DOMTYPEDARRAY_DEFINE_WRAPPERTYPEINFO)
#undef DOMTYPEDARRAY_DEFINE_WRAPPERTYPEINFO

#define DOMTYPEDARRAY_EXPLICITLY_INSTANTIATE(val_t, Type, clamped) \
  template class CORE_TEMPLATE_EXPORT                              \
      DOMTypedArray<val_t, v8::Type##Array, clamped>;
DOMTYPEDARRAY_FOREACH_VIEW_TYPE(DOMTYPEDARRAY_EXPLICITLY_INSTANTIATE)
#undef DOMTYPEDARRAY_EXPLICITLY_INSTANTIATE

#undef DOMTYPEDARRAY_FOREACH_VIEW_TYPE

}  // namespace blink
