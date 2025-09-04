// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_parsing_utils.h"

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"

namespace device::fido_parsing_utils {

namespace {

constexpr bool AreSpansDisjoint(base::span<const uint8_t> lhs,
                                base::span<const uint8_t> rhs) {
  return UNSAFE_TODO(lhs.data() + lhs.size()) <= rhs.data() ||  // [lhs)...[rhs)
         UNSAFE_TODO(rhs.data() + rhs.size()) <= lhs.data();    // [rhs)...[lhs)
}

// Redacts `path` from `cbor` using the semantics described for `RedactCbor`.
// Mutates `cbor` in place.
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
  if (!cbor->is_map()) {
    // Only maps and arrays are supported.
    return;
  }
  cbor::Value::MapValue& map =
      const_cast<cbor::Value::MapValue&>(cbor->GetMap());
  base::span<const cbor::Value> field = path.take_first<1>();
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

std::vector<uint8_t> Materialize(base::span<const uint8_t> span) {
  return std::vector<uint8_t>(span.begin(), span.end());
}

std::optional<std::vector<uint8_t>> MaterializeOrNull(
    std::optional<base::span<const uint8_t>> span) {
  if (span)
    return Materialize(*span);
  return std::nullopt;
}

void Append(std::vector<uint8_t>* target, base::span<const uint8_t> in_values) {
  CHECK(AreSpansDisjoint(*target, in_values));
  target->insert(target->end(), in_values.begin(), in_values.end());
}

std::vector<uint8_t> Extract(base::span<const uint8_t> span,
                             size_t pos,
                             size_t length) {
  return Materialize(ExtractSpan(span, pos, length));
}

base::span<const uint8_t> ExtractSpan(base::span<const uint8_t> span,
                                      size_t pos,
                                      size_t length) {
  if (!(pos <= span.size() && length <= span.size() - pos))
    return base::span<const uint8_t>();
  return span.subspan(pos, length);
}

std::vector<uint8_t> ExtractSuffix(base::span<const uint8_t> span, size_t pos) {
  return Materialize(ExtractSuffixSpan(span, pos));
}

base::span<const uint8_t> ExtractSuffixSpan(base::span<const uint8_t> span,
                                            size_t pos) {
  return span.subspan(std::min(pos, span.size()));
}

std::vector<base::span<const uint8_t>> SplitSpan(base::span<const uint8_t> span,
                                                 size_t max_chunk_size) {
  DCHECK_NE(0u, max_chunk_size);
  std::vector<base::span<const uint8_t>> chunks;
  const size_t num_chunks = (span.size() + max_chunk_size - 1) / max_chunk_size;
  chunks.reserve(num_chunks);
  while (!span.empty()) {
    const size_t chunk_size = std::min(span.size(), max_chunk_size);
    chunks.emplace_back(span.first(chunk_size));
    span = span.subspan(chunk_size);
  }

  return chunks;
}

std::string ConvertBytesToUuid(base::span<const uint8_t, 16> bytes) {
  uint64_t most_significant_bytes = 0;
  for (size_t i = 0; i < sizeof(uint64_t); i++) {
    most_significant_bytes |= base::strict_cast<uint64_t>(bytes[i])
                              << 8 * (7 - i);
  }

  uint64_t least_significant_bytes = 0;
  for (size_t i = 0; i < sizeof(uint64_t); i++) {
    least_significant_bytes |= base::strict_cast<uint64_t>(bytes[i + 8])
                               << 8 * (7 - i);
  }

  return base::StringPrintf(
      "%08x-%04x-%04x-%04x-%012llx",
      static_cast<unsigned int>(most_significant_bytes >> 32),
      static_cast<unsigned int>((most_significant_bytes >> 16) & 0x0000ffff),
      static_cast<unsigned int>(most_significant_bytes & 0x0000ffff),
      static_cast<unsigned int>(least_significant_bytes >> 48),
      least_significant_bytes & 0x0000ffff'ffffffffULL);
}

cbor::Value RedactCbor(
    const cbor::Value& cbor,
    base::span<const std::vector<cbor::Value>> paths_to_redact) {
  cbor::Value response = cbor.Clone();
  for (base::span<const cbor::Value> field_to_redact : paths_to_redact) {
    RedactPath(&response, field_to_redact);
  }
  return response;
}

}  // namespace device::fido_parsing_utils
