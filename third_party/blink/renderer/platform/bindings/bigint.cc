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
