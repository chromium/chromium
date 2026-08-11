// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cbor_util.h"

#include "base/containers/span.h"
#include "components/cbor/values.h"

namespace fido_cbor_util::fido_cbor_util_internal {

namespace {

// Redacts `path` from `cbor` using the semantics described for
// `RedactValueAtPaths`. Mutates `cbor` in place.
void RedactPath(cbor::Value* cbor, base::span<const cbor::Value> path) {
  if (cbor->is_array()) {
    // Mutate all the elements in the array.
    cbor::Value::ArrayValue& array =
        const_cast<cbor::Value::ArrayValue&>(cbor->GetArray());
    for (cbor::Value& value : array) {
      RedactPath(&value, path);
    }
    return;
  }
  if (path.empty() || !cbor->is_map()) {
    // Only maps and arrays are supported.
    return;
  }
  base::span<const cbor::Value> field = path.take_first<1>();
  cbor::Value::MapValue& map =
      const_cast<cbor::Value::MapValue&>(cbor->GetMap());
  const auto it = map.find(field.front());
  if (it == map.end()) {
    // Could not find some part of the path, bail out.
    return;
  }
  if (path.empty()) {
    // Found the leaf, replace the map value regardless of its type.
    it->second = cbor::Value("[redacted]");
    return;
  }
  RedactPath(&it->second, path);
}

}  // namespace

cbor::Value RedactValueAtPathsImpl(
    const cbor::Value& cbor,
    base::span<const base::span<const cbor::Value>> paths_to_redact) {
  cbor::Value response = cbor.Clone();
  for (base::span<const cbor::Value> field_to_redact : paths_to_redact) {
    RedactPath(&response, field_to_redact);
  }
  return response;
}

}  // namespace fido_cbor_util::fido_cbor_util_internal
