// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_ANDROID_GIN_JAVA_BRIDGE_VALUE_H_
#define CONTENT_COMMON_ANDROID_GIN_JAVA_BRIDGE_VALUE_H_

#include <stdint.h>

#include <memory>

#include "base/pickle.h"
#include "base/values.h"
#include "content/common/content_export.h"

// In Java Bridge, we need to pass some kinds of values that can't
// be put into base::Value. And since base::Value is not extensible,
// we transfer these special values via base::Value.

namespace content {

class GinJavaBridgeValue {
 public:
  enum Type {
    TYPE_FIRST_VALUE = 0,
    // JavaScript 'undefined'
    TYPE_UNDEFINED = 0,
    // JavaScript NaN and Infinity
    TYPE_NONFINITE,
    // Bridge Object ID
    TYPE_OBJECT_ID,
    // Uint32 type
    TYPE_UINT32,
    TYPE_LAST_VALUE
  };

  GinJavaBridgeValue(const GinJavaBridgeValue&) = delete;
  GinJavaBridgeValue& operator=(const GinJavaBridgeValue&) = delete;

  // Serialization
  CONTENT_EXPORT static std::unique_ptr<base::Value> CreateUndefinedValue();
  CONTENT_EXPORT static std::unique_ptr<base::Value> CreateNonFiniteValue(
      float in_value);
  CONTENT_EXPORT static std::unique_ptr<base::Value> CreateNonFiniteValue(
      double in_value);
  CONTENT_EXPORT static std::unique_ptr<base::Value> CreateObjectIDValue(
      int32_t in_value);
  CONTENT_EXPORT static std::unique_ptr<base::Value> CreateUInt32Value(
      uint32_t in_value);

  // De-serialization
  CONTENT_EXPORT static bool ContainsGinJavaBridgeValue(
      const base::Value* value);
  CONTENT_EXPORT static std::unique_ptr<const GinJavaBridgeValue> FromValue(
      const base::Value* value);

  CONTENT_EXPORT Type GetType() const;
  CONTENT_EXPORT bool IsType(Type type) const;

  CONTENT_EXPORT bool GetAsNonFinite(float* out_value) const;
  CONTENT_EXPORT bool GetAsObjectID(int32_t* out_object_id) const;
  CONTENT_EXPORT bool GetAsUInt32(uint32_t* out_value) const;

 private:
  explicit GinJavaBridgeValue(Type type);
  explicit GinJavaBridgeValue(const base::Value* value);
  std::unique_ptr<base::Value> SerializeToBinaryValue();

  base::Pickle pickle_;
};

}  // namespace content

#endif  // CONTENT_COMMON_ANDROID_GIN_JAVA_BRIDGE_VALUE_H_
