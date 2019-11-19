// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_MODULES_V8_SERIALIZATION_V8_SCRIPT_VALUE_SERIALIZER_FOR_MODULES_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_MODULES_V8_SERIALIZATION_V8_SCRIPT_VALUE_SERIALIZER_FOR_MODULES_H_

#include "third_party/blink/public/platform/web_crypto_algorithm.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/v8_script_value_serializer.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class NativeFileSystemHandle;
class WebCryptoKey;

// Extends V8ScriptValueSerializer with support for modules/ types.
class MODULES_EXPORT V8ScriptValueSerializerForModules final
    : public V8ScriptValueSerializer {
 public:
  explicit V8ScriptValueSerializerForModules(
      ScriptState* script_state,
      const SerializedScriptValue::SerializeOptions& options)
      : V8ScriptValueSerializer(script_state, options) {}

 protected:
  bool WriteDOMObject(ScriptWrappable*, ExceptionState&) override;

 private:
  void WriteOneByte(uint8_t byte) { WriteRawBytes(&byte, 1); }
  bool WriteCryptoKey(const WebCryptoKey&, ExceptionState&);
  bool WriteNativeFileSystemHandle(
      SerializationTag tag,
      NativeFileSystemHandle* native_file_system_handle);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_MODULES_V8_SERIALIZATION_V8_SCRIPT_VALUE_SERIALIZER_FOR_MODULES_H_
