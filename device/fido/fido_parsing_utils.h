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

// Comparator object that calls std::lexicographical_compare on the begin and
// end iterators of the passed in ranges. Useful when comparing sequence
// containers that are of different types, but have similar semantics.
struct RangeLess {
  template <typename T, typename U>
  constexpr bool operator()(T&& lhs, U&& rhs) const {
    using std::begin;
    using std::end;
    return std::lexicographical_compare(begin(lhs), end(lhs), begin(rhs),
                                        end(rhs));
  }

  using is_transparent = void;
};

// Returns a materialized copy of |span|, that is, a vector with the same
// elements.
COMPONENT_EXPORT(DEVICE_FIDO)
std::vector<uint8_t> Materialize(base::span<const uint8_t> span);
COMPONENT_EXPORT(DEVICE_FIDO)
std::optional<std::vector<uint8_t>> MaterializeOrNull(
    std::optional<base::span<const uint8_t>> span);

// Returns a materialized copy of the static |span|, that is, an array with the
// same elements.
template <size_t N>
std::array<uint8_t, N> Materialize(base::span<const uint8_t, N> span) {
  std::array<uint8_t, N> array;
  std::ranges::copy(span, array.begin());
  return array;
}

// Appends |in_values| to the end of |target|. The underlying container for
// |in_values| should *not* be |target|.
COMPONENT_EXPORT(DEVICE_FIDO)
void Append(std::vector<uint8_t>* target, base::span<const uint8_t> in_values);

// Safely extracts, with bound checking, a contiguous subsequence of |span| of
// the given |length| and starting at |pos|. Returns an empty vector/span if the
// requested range is out-of-bound.
COMPONENT_EXPORT(DEVICE_FIDO)
std::vector<uint8_t> Extract(base::span<const uint8_t> span,
                             size_t pos,
                             size_t length);
COMPONENT_EXPORT(DEVICE_FIDO)
base::span<const uint8_t> ExtractSpan(base::span<const uint8_t> span,
                                      size_t pos,
                                      size_t length);

// Safely extracts, with bound checking, the suffix of the given |span| starting
// at the given position |pos|. Returns an empty vector/span if the requested
// starting position is out-of-bound.
COMPONENT_EXPORT(DEVICE_FIDO)
std::vector<uint8_t> ExtractSuffix(base::span<const uint8_t> span, size_t pos);

COMPONENT_EXPORT(DEVICE_FIDO)
base::span<const uint8_t> ExtractSuffixSpan(base::span<const uint8_t> span,
                                            size_t pos);

template <size_t N>
bool ExtractArray(base::span<const uint8_t> span,
                  size_t pos,
                  std::array<uint8_t, N>* array) {
  const auto extracted_span = ExtractSpan(span, pos, N);
  if (extracted_span.size() != N)
    return false;

  std::ranges::copy(extracted_span, array->begin());
  return true;
}

// Partitions |span| into N = ⌈span.size() / max_chunk_size⌉ consecutive chunks.
// The first N-1 chunks are of size |max_chunk_size|, and the Nth chunk is of
// size span.size() % max_chunk_size. |max_chunk_size| must be greater than 0.
// Returns an empty vector in case |span| is empty.
COMPONENT_EXPORT(DEVICE_FIDO)
std::vector<base::span<const uint8_t>> SplitSpan(base::span<const uint8_t> span,
                                                 size_t max_chunk_size);

// Convert byte array into GUID formatted string as defined by RFC 4122.
// As we are converting 128 bit UUID, |bytes| must be have length of 16.
// https://tools.ietf.org/html/rfc4122
COMPONENT_EXPORT(DEVICE_FIDO)
std::string ConvertBytesToUuid(base::span<const uint8_t, 16> bytes);

// Copies the contents of the bytestring, keyed by |key|, from |map| into |out|.
// Returns true on success or false if the key if not found, the value is not a
// bytestring, or the value has the wrong length.
template <size_t N>
bool CopyCBORBytestring(std::array<uint8_t, N>* out,
                        const cbor::Value::MapValue& map,
                        int key) {
  const auto it = map.find(cbor::Value(key));
  if (it == map.end() || !it->second.is_bytestring()) {
    return false;
  }
  const std::vector<uint8_t> bytestring = it->second.GetBytestring();
  return ExtractArray(bytestring, /*pos=*/0, out);
}

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
