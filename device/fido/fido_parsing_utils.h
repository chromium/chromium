// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FIDO_PARSING_UTILS_H_
#define DEVICE_FIDO_FIDO_PARSING_UTILS_H_

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <array>
#include <iterator>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "components/cbor/values.h"

namespace device::fido_parsing_utils {

// Redacts `paths_to_redact` from `cbor` by finding the corresponding keys and
// replacing them by the cbor string "redacted". Nested paths should correspond
// to nested maps under the same key name. The redaction is applied to all array
// elements for a matching key.
// If a path is not found, a clone of `cbor` is returned.
//
// Example:
//
// Given a `cbor` value...
// {
//   characters: [
//     {
//       name: "Reimu",
//       occupation: ["Shrine maiden"]
//     },
//     {
//       name: "Marisa",
//       occupation: ["Witch", "Troublemaker"]
//     }
//   ]
// }
//
// ...and a `paths_to_redact` value...
//
// [
//   ["characters", "occupation"],
//   ["characters", "date-of-birth"],
// ]
//
// ...the returned cbor will be:
//
// {
//   characters: [
//     {
//       name: "Reimu",
//       occupation: "redacted"
//     },
//     {
//       name: "Marisa",
//       occupation: "redacted"
//     }
//   ]
// }
COMPONENT_EXPORT(DEVICE_FIDO)
cbor::Value RedactCbor(
    const cbor::Value& cbor,
    base::span<const std::vector<cbor::Value>> paths_to_redact);

namespace internal {

// These helpers allow us to implement `ToCborVector` without needing to prepend
// values to the vector to satisfy the left recursion, which would be slower.
template <typename T>
void AppendToCborVector(std::vector<cbor::Value>* vec, T value) {
  vec->emplace_back(std::move(value));
}
template <typename T, typename... Args>
void AppendToCborVector(std::vector<cbor::Value>* vec, T first, Args... args) {
  vec->emplace_back(std::move(first));
  AppendToCborVector(vec, args...);
}

}  // namespace internal

// This function makes using `RedactCbor` more readable by moving the parameters
// into a vector of `cbor::Value`s and returning it.
template <typename... Args>
std::vector<cbor::Value> ToCborVector(Args... args) {
  std::vector<cbor::Value> vec;
  internal::AppendToCborVector(&vec, args...);
  return vec;
}

}  // namespace device::fido_parsing_utils

#endif  // DEVICE_FIDO_FIDO_PARSING_UTILS_H_
