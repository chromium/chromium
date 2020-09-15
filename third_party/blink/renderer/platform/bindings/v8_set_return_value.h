// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_SET_RETURN_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_SET_RETURN_VALUE_H_

#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/bindings/dom_data_store.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/bindings/v8_value_cache.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "v8/include/v8.h"

namespace blink {

namespace bindings {

// V8SetReturnValue sets a return value in a V8 callback function.  The first
// two arguments are fixed as v8::{Function,Property}CallbackInfo and the
// return value.  V8SetReturnValue may take more arguments as optimization hints
// depending on the return value type.

struct V8ReturnValue {
  STATIC_ONLY(V8ReturnValue);

  // Support compile-time overload resolution by making each value have its own
  // type.

  // Applies strict typing to IDL primitive types.
  template <typename T>
  struct PrimitiveType {};

  // Nullable or not
  enum NonNullable { kNonNullable };
  enum Nullable { kNullable };

  // FrozenArray or not (the integrity level = frozen or not)
  enum Frozen { kFrozen };

  // Main world or not
  enum MainWorld { kMainWorld };

  // Returns the interface object of the given type.
  enum InterfaceObject { kInterfaceObject };

  // Selects the appropriate creation context.
  static v8::Local<v8::Object> CreationContext(
      const v8::FunctionCallbackInfo<v8::Value>& info) {
    return info.This();
  }
  static v8::Local<v8::Object> CreationContext(
      const v8::PropertyCallbackInfo<v8::Value>& info) {
    return info.Holder();
  }
};

// V8 handle types
template <typename CallbackInfo, typename S>
void V8SetReturnValue(const CallbackInfo& info, const v8::Local<S> value) {
  info.GetReturnValue().Set(value);
}

template <typename CallbackInfo, typename S>
void V8SetReturnValue(const CallbackInfo& info,
                      const v8::Local<S> value,
                      V8ReturnValue::Frozen) {
  if (value->IsObject()) {
    bool result =
        value.template As<v8::Object>()
            ->SetIntegrityLevel(info.GetIsolate()->GetCurrentContext(),
                                v8::IntegrityLevel::kFrozen)
            .ToChecked();
    CHECK(result);
  }
  info.GetReturnValue().Set(value);
}

// Property descriptor
PLATFORM_EXPORT v8::Local<v8::Object> CreatePropertyDescriptorObject(
    v8::Isolate* isolate,
    const v8::PropertyDescriptor& desc);

PLATFORM_EXPORT inline void V8SetReturnValue(
    const v8::PropertyCallbackInfo<v8::Value>& info,
    const v8::PropertyDescriptor& value) {
  info.GetReturnValue().Set(
      CreatePropertyDescriptorObject(info.GetIsolate(), value));
}

// Indexed properties and named properties
PLATFORM_EXPORT inline void V8SetReturnValue(
    const v8::FunctionCallbackInfo<v8::Value>& info,
    IndexedPropertySetterResult value) {
  // If an operation implementing indexed property setter is invoked as a
  // regular operation, and the return type is not type void (V8SetReturnValue
  // won't be called in case of type void), then return the given value as is.
  info.GetReturnValue().Set(info[1]);
}

PLATFORM_EXPORT inline void V8SetReturnValue(
    const v8::PropertyCallbackInfo<v8::Value>& info,
    IndexedPropertySetterResult value) {
  if (value == IndexedPropertySetterResult::kDidNotIntercept) {
    // Do not set the return value to indicate that the request was not
    // intercepted.
    return;
  }
  info.GetReturnValue().SetNull();
}

PLATFORM_EXPORT inline void V8SetReturnValue(
    const v8::FunctionCallbackInfo<v8::Value>& info,
    NamedPropertySetterResult value) {
  // If an operation implementing named property setter is invoked as a
  // regular operation, and the return type is not type void (V8SetReturnValue
  // won't be called in case of type void), then return the given value as is.
  info.GetReturnValue().Set(info[1]);
}

PLATFORM_EXPORT inline void V8SetReturnValue(
    const v8::PropertyCallbackInfo<v8::Value>& info,
    NamedPropertySetterResult value) {
  if (value == NamedPropertySetterResult::kDidNotIntercept) {
    // Do not set the return value to indicate that the request was not
    // intercepted.
    return;
  }
  info.GetReturnValue().SetNull();
}

PLATFORM_EXPORT inline void V8SetReturnValue(
    const v8::PropertyCallbackInfo<v8::Boolean>& info,
    NamedPropertyDeleterResult value) {
  if (value == NamedPropertyDeleterResult::kDidNotIntercept) {
    // Do not set the return value to indicate that the request was not
    // intercepted.
    return;
  }
  info.GetReturnValue().Set(value == NamedPropertyDeleterResult::kDeleted);
}

// nullptr
template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info, std::nullptr_t) {
  info.GetReturnValue().SetNull();
}

// Primitive types
template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info, bool value) {
  info.GetReturnValue().Set(value);
}

template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info, int32_t value) {
  info.GetReturnValue().Set(value);
}

template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info, uint32_t value) {
  info.GetReturnValue().Set(value);
}

template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info, int64_t value) {
  // ECMAScript doesn't support 64-bit integer in Number type.
  info.GetReturnValue().Set(static_cast<double>(value));
}

template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info, uint64_t value) {
  // ECMAScript doesn't support 64-bit integer in Number type.
  info.GetReturnValue().Set(static_cast<double>(value));
}

template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info, double value) {
  info.GetReturnValue().Set(value);
}

// Primitive types with IDL type
//
// |IdlType| represents a C++ type corresponding to an IDL type, and |value| is
// passed from Blink implementation and its type occasionally does not match
// the IDL type because Blink is not always respectful to IDL types.  These
// functions fix such a type mismatch.
template <typename CallbackInfo, typename BlinkType, typename IdlType>
inline typename std::enable_if_t<std::is_arithmetic<BlinkType>::value ||
                                 std::is_enum<BlinkType>::value>
V8SetReturnValue(const CallbackInfo& info,
                 BlinkType value,
                 V8ReturnValue::PrimitiveType<IdlType>) {
  V8SetReturnValue(info, IdlType(value));
}

template <typename CallbackInfo, typename BlinkType>
inline void V8SetReturnValue(const CallbackInfo& info,
                             BlinkType* value,
                             V8ReturnValue::PrimitiveType<bool>) {
  V8SetReturnValue(info, bool(value));
}

// String types
template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info,
                      const AtomicString& string,
                      v8::Isolate* isolate,
                      V8ReturnValue::NonNullable) {
  if (string.IsNull())
    return info.GetReturnValue().SetEmptyString();
  V8PerIsolateData::From(isolate)->GetStringCache()->SetReturnValueFromString(
      info.GetReturnValue(), string.Impl());
}

template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info,
                      const String& string,
                      v8::Isolate* isolate,
                      V8ReturnValue::NonNullable) {
  if (string.IsNull())
    return info.GetReturnValue().SetEmptyString();
  V8PerIsolateData::From(isolate)->GetStringCache()->SetReturnValueFromString(
      info.GetReturnValue(), string.Impl());
}

template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info,
                      const WebString& string,
                      v8::Isolate* isolate,
                      V8ReturnValue::NonNullable) {
  if (string.IsNull())
    return info.GetReturnValue().SetEmptyString();
  V8PerIsolateData::From(isolate)->GetStringCache()->SetReturnValueFromString(
      info.GetReturnValue(), static_cast<String>(string).Impl());
}

template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info,
                      const AtomicString& string,
                      v8::Isolate* isolate,
                      V8ReturnValue::Nullable) {
  if (string.IsNull())
    return info.GetReturnValue().SetNull();
  V8PerIsolateData::From(isolate)->GetStringCache()->SetReturnValueFromString(
      info.GetReturnValue(), string.Impl());
}

template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info,
                      const String& string,
                      v8::Isolate* isolate,
                      V8ReturnValue::Nullable) {
  if (string.IsNull())
    return info.GetReturnValue().SetNull();
  V8PerIsolateData::From(isolate)->GetStringCache()->SetReturnValueFromString(
      info.GetReturnValue(), string.Impl());
}

template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info,
                      const WebString& string,
                      v8::Isolate* isolate,
                      V8ReturnValue::Nullable) {
  if (string.IsNull())
    return info.GetReturnValue().SetNull();
  V8PerIsolateData::From(isolate)->GetStringCache()->SetReturnValueFromString(
      info.GetReturnValue(), static_cast<String>(string).Impl());
}

// ScriptWrappable
template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info,
                      const ScriptWrappable* value,
                      V8ReturnValue::MainWorld) {
  DCHECK(DOMWrapperWorld::Current(info.GetIsolate()).IsMainWorld());
  if (UNLIKELY(!value))
    return info.GetReturnValue().SetNull();

  ScriptWrappable* wrappable = const_cast<ScriptWrappable*>(value);
  if (DOMDataStore::SetReturnValueForMainWorld(info.GetReturnValue(),
                                               wrappable))
    return;

  info.GetReturnValue().Set(
      wrappable->Wrap(info.GetIsolate(), V8ReturnValue::CreationContext(info)));
}

template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info,
                      const ScriptWrappable& value,
                      V8ReturnValue::MainWorld) {
  DCHECK(DOMWrapperWorld::Current(info.GetIsolate()).IsMainWorld());
  ScriptWrappable* wrappable = const_cast<ScriptWrappable*>(&value);
  if (DOMDataStore::SetReturnValueForMainWorld(info.GetReturnValue(),
                                               wrappable))
    return;

  info.GetReturnValue().Set(
      wrappable->Wrap(info.GetIsolate(), V8ReturnValue::CreationContext(info)));
}

template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info,
                      const ScriptWrappable* value,
                      const ScriptWrappable* receiver) {
  if (UNLIKELY(!value))
    return info.GetReturnValue().SetNull();

  ScriptWrappable* wrappable = const_cast<ScriptWrappable*>(value);
  if (DOMDataStore::SetReturnValueFast(info.GetReturnValue(), wrappable,
                                       V8ReturnValue::CreationContext(info),
                                       receiver)) {
    return;
  }

  info.GetReturnValue().Set(
      wrappable->Wrap(info.GetIsolate(), V8ReturnValue::CreationContext(info)));
}

template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info,
                      const ScriptWrappable& value,
                      const ScriptWrappable* receiver) {
  ScriptWrappable* wrappable = const_cast<ScriptWrappable*>(&value);
  if (DOMDataStore::SetReturnValueFast(info.GetReturnValue(), wrappable,
                                       V8ReturnValue::CreationContext(info),
                                       receiver)) {
    return;
  }

  info.GetReturnValue().Set(
      wrappable->Wrap(info.GetIsolate(), V8ReturnValue::CreationContext(info)));
}

template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info,
                      const ScriptWrappable* value,
                      v8::Local<v8::Context> creation_context) {
  if (UNLIKELY(!value))
    return info.GetReturnValue().SetNull();

  ScriptWrappable* wrappable = const_cast<ScriptWrappable*>(value);
  if (DOMDataStore::SetReturnValue(info.GetReturnValue(), wrappable))
    return;

  info.GetReturnValue().Set(
      wrappable->Wrap(info.GetIsolate(), creation_context->Global()));
}

template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info,
                      ScriptWrappable& value,
                      v8::Local<v8::Context> creation_context) {
  ScriptWrappable* wrappable = const_cast<ScriptWrappable*>(&value);
  if (DOMDataStore::SetReturnValue(info.GetReturnValue(), wrappable))
    return;

  info.GetReturnValue().Set(
      wrappable->Wrap(info.GetIsolate(), creation_context->Global()));
}

// Interface object
PLATFORM_EXPORT v8::Local<v8::Value> GetInterfaceObjectExposedOnGlobal(
    v8::Isolate* isolate,
    v8::Local<v8::Object> creation_context,
    const WrapperTypeInfo* wrapper_type_info);

inline void V8SetReturnValue(const v8::PropertyCallbackInfo<v8::Value>& info,
                             const WrapperTypeInfo* wrapper_type_info,
                             V8ReturnValue::InterfaceObject) {
  info.GetReturnValue().Set(GetInterfaceObjectExposedOnGlobal(
      info.GetIsolate(), info.Holder(), wrapper_type_info));
}

// Nullable types
template <typename CallbackInfo, typename T, typename... ExtraArgs>
void V8SetReturnValue(const CallbackInfo& info,
                      base::Optional<T> value,
                      ExtraArgs... extra_args) {
  if (value.has_value()) {
    V8SetReturnValue(info, value.value(),
                     std::forward<ExtraArgs>(extra_args)...);
  } else {
    info.GetReturnValue().SetNull();
  }
}

}  // namespace bindings

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_SET_RETURN_VALUE_H_
