// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_INTERFACE_BRIDGE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_INTERFACE_BRIDGE_H_

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/v8_interface_bridge_base.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"

namespace blink {

namespace bindings {

template <class V8T, class T>
class V8InterfaceBridge : public V8InterfaceBridgeBase {
 public:
  static T* ToWrappable(v8::Isolate* isolate, v8::Local<v8::Value> value) {
    return HasInstance(isolate, value)
               ? ToWrappableUnsafe(value.As<v8::Object>())
               : nullptr;
  }

  static T* ToWrappableUnsafe(v8::Local<v8::Object> value) {
    return ToScriptWrappable(value)->ToImpl<T>();
  }

  static bool HasInstance(v8::Isolate* isolate, v8::Local<v8::Value> value) {
    return V8PerIsolateData::From(isolate)->HasInstance(
        V8T::GetWrapperTypeInfo(), value);
  }

  // Migration adapter
  static bool HasInstance(v8::Local<v8::Value> value, v8::Isolate* isolate) {
    return HasInstance(isolate, value);
  }

  static T* ToImpl(v8::Local<v8::Object> value) {
    return ToWrappableUnsafe(value);
  }

  static T* ToImplWithTypeCheck(v8::Isolate* isolate,
                                v8::Local<v8::Value> value) {
    return ToWrappable(isolate, value);
  }
};

}  // namespace bindings

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_INTERFACE_BRIDGE_H_
