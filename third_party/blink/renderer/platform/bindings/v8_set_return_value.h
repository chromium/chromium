// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_SET_RETURN_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_SET_RETURN_VALUE_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/bindings/dom_data_store.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/enumeration_base.h"
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

  // Main world or not
  enum MainWorld { kMainWorld };

  // The return value can be a cross origin object.
  enum MaybeCrossOrigin { kMaybeCrossOrigin };

  // Returns the exposed object of the given type.
  enum InterfaceObject { kInterfaceObject };
  enum NamespaceObject { kNamespaceObject };

  // Selects the appropriate creation context.
  static v8::Local<v8::Object> CreationContext(
      const v8::FunctionCallbackInfo<v8::Value>& info) {
    return info.This();
  }
  static v8::Local<v8::Object> CreationContext(
      const v8::PropertyCallbackInfo<v8::Value>& info) {
    return info.Holder();
  }

  // Helper function for ScriptWrappable
  template <typename CallbackInfo>
  static void SetWrapper(const CallbackInfo& info,
                         ScriptWrappable* wrappable,
                         v8::Local<v8::Context> creation_context) {
    v8::Local<v8::Value> wrapper;
    if (!wrappable->Wrap(ScriptState::From(creation_context))
             .ToLocal(&wrapper)) {
      return;
    }
    info.GetReturnValue().Set(wrapper);
  }
};

// V8 handle types
template <typename CallbackInfo, typename S, typename... ExtraArgs>
void V8SetReturnValue(const CallbackInfo& info,
                      const v8::Local<S> value,
                      ExtraArgs... extra_args) {
  info.GetReturnValue().Set(value);
}

// Property descriptor
PLATFORM_EXPORT v8::Local<v8::Object> CreatePropertyDescriptorObject(
    v8::Isolate* isolate,
    const v8::PropertyDescriptor& desc);

template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info,
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
  V8ReturnValue::SetWrapper(
      info, wrappable,
      V8ReturnValue::CreationContext(info)->GetCreationContextChecked());
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
  V8ReturnValue::SetWrapper(
      info, wrappable,
      V8ReturnValue::CreationContext(info)->GetCreationContextChecked());
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
  V8ReturnValue::SetWrapper(
      info, wrappable,
      V8ReturnValue::CreationContext(info)->GetCreationContextChecked());
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
  V8ReturnValue::SetWrapper(
      info, wrappable,
      V8ReturnValue::CreationContext(info)->GetCreationContextChecked());
}

template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info,
                      const ScriptWrappable* value,
                      const ScriptWrappable* receiver,
                      V8ReturnValue::MaybeCrossOrigin) {
  if (UNLIKELY(!value))
    return info.GetReturnValue().SetNull();
  ScriptWrappable* wrappable = const_cast<ScriptWrappable*>(value);
  if (DOMDataStore::SetReturnValueFast(info.GetReturnValue(), wrappable,
                                       V8ReturnValue::CreationContext(info),
                                       receiver)) {
    return;
  }
  // Check whether the creation context is available, and if not, use the
  // current context. When a cross-origin Window is associated with a
  // v8::Context::NewRemoteContext(), there is no creation context in the usual
  // sense. It's ok to use the current context in that case because:
  // 1) The Window objects must have their own creation context and must never
  //    need a creation context to be specified.
  // 2) Even though a v8::Context is not necessary in case
  //    of Window objects, v8::Isolate and DOMWrapperWorld are still necessary
  //    to create an appropriate wrapper object, and the ScriptState associated
  //    with the current context will still have the correct v8::Isolate and
  //    DOMWrapperWorld.
  v8::Local<v8::Context> context;
  if (!V8ReturnValue::CreationContext(info)->GetCreationContext().ToLocal(
          &context)) {
    context = info.GetIsolate()->GetCurrentContext();
  }
  V8ReturnValue::SetWrapper(info, wrappable, context);
}

template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info,
                      const ScriptWrappable& value,
                      const ScriptWrappable* receiver,
                      V8ReturnValue::MaybeCrossOrigin) {
  ScriptWrappable* wrappable = const_cast<ScriptWrappable*>(&value);
  if (DOMDataStore::SetReturnValueFast(info.GetReturnValue(), wrappable,
                                       V8ReturnValue::CreationContext(info),
                                       receiver)) {
    return;
  }
  // Check whether the creation context is available, and if not, use the
  // current context. When a cross-origin Window is associated with a
  // v8::Context::NewRemoteContext(), there is no creation context in the usual
  // sense. It's ok to use the current context in that case because:
  // 1) The Window objects must have their own creation context and must never
  //    need a creation context to be specified.
  // 2) Even though a v8::Context is not necessary in case
  //    of Window objects, v8::Isolate and DOMWrapperWorld are still necessary
  //    to create an appropriate wrapper object, and the ScriptState associated
  //    with the current context will still have the correct v8::Isolate and
  //    DOMWrapperWorld.
  v8::Local<v8::Context> context;
  if (!V8ReturnValue::CreationContext(info)->GetCreationContext().ToLocal(
          &context)) {
    context = info.GetIsolate()->GetCurrentContext();
  }
  V8ReturnValue::SetWrapper(info, wrappable, context);
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
  V8ReturnValue::SetWrapper(info, wrappable, creation_context);
}

template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info,
                      ScriptWrappable& value,
                      v8::Local<v8::Context> creation_context) {
  ScriptWrappable* wrappable = const_cast<ScriptWrappable*>(&value);
  if (DOMDataStore::SetReturnValue(info.GetReturnValue(), wrappable))
    return;
  V8ReturnValue::SetWrapper(info, wrappable, creation_context);
}

// EnumerationBase
template <typename CallbackInfo, typename... ExtraArgs>
void V8SetReturnValue(const CallbackInfo& info,
                      const bindings::EnumerationBase& value,
                      v8::Isolate* isolate,
                      ExtraArgs... extra_args) {
  V8PerIsolateData::From(isolate)->GetStringCache()->SetReturnValueFromString(
      info.GetReturnValue(), value.AsString().Impl());
}

// Nullable types
template <typename CallbackInfo, typename T, typename... ExtraArgs>
void V8SetReturnValue(const CallbackInfo& info,
                      absl::optional<T> value,
                      ExtraArgs... extra_args) {
  if (value.has_value()) {
    V8SetReturnValue(info, value.value(),
                     std::forward<ExtraArgs>(extra_args)...);
  } else {
    info.GetReturnValue().SetNull();
  }
}

// Exposed objects
PLATFORM_EXPORT v8::Local<v8::Value> GetExposedInterfaceObject(
    v8::Isolate* isolate,
    v8::Local<v8::Object> creation_context,
    const WrapperTypeInfo* wrapper_type_info);

PLATFORM_EXPORT v8::Local<v8::Value> GetExposedNamespaceObject(
    v8::Isolate* isolate,
    v8::Local<v8::Object> creation_context,
    const WrapperTypeInfo* wrapper_type_info);

inline void V8SetReturnValue(const v8::PropertyCallbackInfo<v8::Value>& info,
                             const WrapperTypeInfo* wrapper_type_info,
                             V8ReturnValue::InterfaceObject) {
  info.GetReturnValue().Set(GetExposedInterfaceObject(
      info.GetIsolate(), info.Holder(), wrapper_type_info));
}

inline void V8SetReturnValue(const v8::PropertyCallbackInfo<v8::Value>& info,
                             const WrapperTypeInfo* wrapper_type_info,
                             V8ReturnValue::NamespaceObject) {
  info.GetReturnValue().Set(GetExposedNamespaceObject(
      info.GetIsolate(), info.Holder(), wrapper_type_info));
}

}  // namespace bindings

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_SET_RETURN_VALUE_H_
