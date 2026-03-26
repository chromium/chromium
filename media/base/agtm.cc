// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/agtm.h"

#include <array>

#include "third_party/skia/include/core/SkData.h"

namespace media {

constexpr auto kAgtmPrefix = std::to_array<uint8_t>(
    {0xB5,          // Country code (US)
     0x00, 0x90,    // Terminal provider code (SMPTE)
     0x00, 0x01});  // Terminal provider oriented code (DMCVT Application #5)

bool MatchesAgtmT35(base::span<const uint8_t> t35_prefix) {
  return t35_prefix == base::span(kAgtmPrefix);
}

std::optional<base::span<const uint8_t>> GetAgtmFromT35WithCountryCode(
    uint8_t t35_country_code,
    base::span<const uint8_t> t35_payload_without_country_code) {
  if (t35_country_code != kAgtmPrefix[0]) {
    return std::nullopt;
  }
  auto expected_prefix = base::span(kAgtmPrefix).subspan(1u);
  if (t35_payload_without_country_code.size() < expected_prefix.size()) {
    return std::nullopt;
  }
  auto prefix = t35_payload_without_country_code.first(expected_prefix.size());
  if (prefix != expected_prefix) {
    return std::nullopt;
  }
  return t35_payload_without_country_code.subspan(prefix.size());
}

std::optional<base::span<const uint8_t>> GetAgtmFromT35(
    base::span<const uint8_t> t35_payload) {
  auto expected_prefix = base::span(kAgtmPrefix);
  if (t35_payload.size() < expected_prefix.size()) {
    return std::nullopt;
  }
  auto prefix = t35_payload.first(expected_prefix.size());
  if (prefix != expected_prefix) {
    return std::nullopt;
  }
  return t35_payload.subspan(prefix.size());
}

}  // namespace media
