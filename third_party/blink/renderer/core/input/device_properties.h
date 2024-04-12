// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INPUT_DEVICE_PROPERTIES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INPUT_DEVICE_PROPERTIES_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_device_properties_init.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {
class CORE_EXPORT DeviceProperties final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DeviceProperties* Create(const DevicePropertiesInit* initializer) {
    return MakeGarbageCollected<DeviceProperties>(initializer);
  }
  explicit DeviceProperties(const DevicePropertiesInit* initializer)
      : unique_id_(initializer->uniqueId()) {}

  int32_t uniqueId() const { return unique_id_; }

  void Trace(Visitor* visitor) const override {
    ScriptWrappable::Trace(visitor);
  }

 private:
  int32_t unique_id_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INPUT_DEVICE_PROPERTIES_H_
