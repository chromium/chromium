// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CBOR_UTIL_H_
#define DEVICE_FIDO_CBOR_UTIL_H_

#include <array>
#include <utility>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "components/cbor/values.h"

namespace fido_cbor_util {

namespace fido_cbor_util_internal {

COMPONENT_EXPORT(DEVICE_FIDO)
cbor::Value RedactValueAtPathsImpl(
    const cbor::Value& cbor,
    base::span<const base::span<const cbor::Value>> paths_to_redact);

}  // namespace fido_cbor_util_internal

// Redacts `paths` from `cbor` by finding the corresponding keys and replacing
// them by the cbor string "[redacted]". Nested paths should correspond to
// nested maps under the same key name. The redaction is applied to all array
// elements for a matching key. If a path is not found, a clone of `cbor` is
// returned. Use the `Path()` helper to construct `paths`.
template <typename... ArrayPaths>
cbor::Value RedactValueAtPaths(const cbor::Value& cbor,
                               const ArrayPaths&... paths) {
  const base::span<const cbor::Value> spans[] = {paths...};
  return fido_cbor_util_internal::RedactValueAtPathsImpl(cbor, spans);
}

// Helper to construct a redaction path to pass to `RedactValueAtPaths()`.
template <typename... Args>
std::array<cbor::Value, sizeof...(Args)> Path(Args&&... args) {
  return {cbor::Value(std::forward<Args>(args))...};
}

}  // namespace fido_cbor_util

#endif  // DEVICE_FIDO_CBOR_UTIL_H_
