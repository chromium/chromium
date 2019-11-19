// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SERIALIZATION_SERIALIZED_SCRIPT_VALUE_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SERIALIZATION_SERIALIZED_SCRIPT_VALUE_FACTORY_H_

#include "base/macros.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class CORE_EXPORT SerializedScriptValueFactory {
  USING_FAST_MALLOC(SerializedScriptValueFactory);

 public:
  // SerializedScriptValueFactory::initialize() should be invoked when Blink is
  // initialized, i.e. initialize() in WebKit.cpp.
  static void Initialize(SerializedScriptValueFactory* new_factory) {
    DCHECK(!instance_);
    instance_ = new_factory;
  }

 protected:
  friend class SerializedScriptValue;
  friend class UnpackedSerializedScriptValue;

  // Following 2 methods are expected to be called by SerializedScriptValue.

  // If a serialization error occurs (e.g., cyclic input value) this
  // function returns an empty representation, schedules a V8 exception to
  // be thrown using v8::ThrowException(), and sets |didThrow|. In this case
  // the caller must not invoke any V8 operations until control returns to
  // V8. When serialization is successful, |didThrow| is false.
  virtual scoped_refptr<SerializedScriptValue> Create(
      v8::Isolate*,
      v8::Local<v8::Value>,
      const SerializedScriptValue::SerializeOptions&,
      ExceptionState&);

  virtual v8::Local<v8::Value> Deserialize(
      scoped_refptr<SerializedScriptValue>,
      v8::Isolate*,
      const SerializedScriptValue::DeserializeOptions&);

  virtual v8::Local<v8::Value> Deserialize(
      UnpackedSerializedScriptValue*,
      v8::Isolate*,
      const SerializedScriptValue::DeserializeOptions&);

  // Following methods are expected to be called in
  // SerializedScriptValueFactory{ForModules}.
  SerializedScriptValueFactory() = default;

 private:
  static SerializedScriptValueFactory& Instance() {
    if (!instance_) {
      NOTREACHED();
      instance_ = new SerializedScriptValueFactory;
    }
    return *instance_;
  }

  static SerializedScriptValueFactory* instance_;

  DISALLOW_COPY_AND_ASSIGN(SerializedScriptValueFactory);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SERIALIZATION_SERIALIZED_SCRIPT_VALUE_FACTORY_H_
