// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/bigint.h"

#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-value.h"

namespace blink {

// This implementation is based on `v8::internal::BigInt::AsInt64`.
// https://source.chromium.org/chromium/chromium/src/+/main:v8/src/objects/bigint.cc;drc=17252ebab8c8f8977b19616706fa98dcf4d9d7ae;l=1443
std::optional<int64_t> BigInt::ToInt64() const {
  if (words_.empty()) {
    return 0;
  }
  if (words_.size() != 1) {
    return std::nullopt;
  }
  uint64_t raw = words_[0];
  // Simulate two's complement.
  raw = IsNegative() ? ((~raw) + 1u) : raw;
  int64_t result = static_cast<int64_t>(raw);
  if ((result < 0) != IsNegative()) {
    return std::nullopt;
  }
  return result;
}

// This implementation is based on `v8::Internal::BigInt::AsUint64`.
// https://source.chromium.org/chromium/chromium/src/+/main:v8/src/objects/bigint.cc;drc=17252ebab8c8f8977b19616706fa98dcf4d9d7ae;l=1450
std::optional<uint64_t> BigInt::ToUInt64() const {
  if (words_.empty()) {
    return 0;
  }
  if (IsNegative() || words_.size() != 1) {
    return std::nullopt;
  }
  return words_[0];
}

BigInt ToBigIntSlow(v8::Isolate* isolate,
                    v8::Local<v8::Value> value,
                    ExceptionState& exception_state) {
  DCHECK(!value->IsBigInt());
  if (!RuntimeEnabledFeatures::WebIDLBigIntUsesToBigIntEnabled()) {
    exception_state.ThrowTypeError("The provided value is not a BigInt.");
    return BigInt();
  }

  TryRethrowScope rethrow_scope(isolate, exception_state);
  v8::Local<v8::BigInt> bigint_value;
  if (!value->ToBigInt(isolate->GetCurrentContext()).ToLocal(&bigint_value)) {
    return BigInt();
  }
  return BigInt(bigint_value);
}

}  // namespace blink
