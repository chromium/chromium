// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_BLUETOOTH_BLUETOOTH_UUID_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_BLUETOOTH_BLUETOOTH_UUID_H_

#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class ExceptionState;

// This class provides a way for script to look up UUIDs by name so they don't
// need to be replicated in each application.
class BluetoothUUID final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // IDL exposed interface:
  static String getService(const V8BluetoothServiceUUID* name,
                           ExceptionState& exception_state);
  static String getCharacteristic(const V8BluetoothCharacteristicUUID* name,
                                  ExceptionState& exception_state);
  static String getDescriptor(const V8BluetoothDescriptorUUID* name,
                              ExceptionState& exception_state);
  static String canonicalUUID(unsigned alias);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_BLUETOOTH_BLUETOOTH_UUID_H_
