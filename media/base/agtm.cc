// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/agtm.h"

#include "third_party/skia/include/core/SkData.h"

namespace media {

// Returns AGTM metadata if the ITU-T T.35 message contains some.
sk_sp<const SkData> GetSerializedAgtmItutT35(
    uint8_t t35_country_code,
    base::span<const uint8_t> t35_payload) {
  // The size of the ITU-T T.35 header.
  static constexpr size_t kItuT35HeaderSize = 4;
  // The minimum body size needed for valid Agtm metadata.
  static constexpr size_t kAgtmMinSize = 2;
  static constexpr uint8_t kItuT35USCountryCode = 0xB5;

  // Defined in SMPTE ST 2094-50: Annex D and Annex C (C.2.1).
  const bool is_agtm = t35_country_code == kItuT35USCountryCode &&
                       t35_payload.size() >= kItuT35HeaderSize + kAgtmMinSize &&
                       // itu_t_t35_us_terminal_provider_code u(16)
                       t35_payload[0] == 0x00 && t35_payload[1] == 0x90 &&
                       // itu_t_t35_smpte_terminal_provider_oriented_code u(16)
                       t35_payload[2] == 0x00 && t35_payload[3] == 0x01;
  if (!is_agtm) {
    return nullptr;
  }
  const auto agtm_payload_span = t35_payload.subspan(kItuT35HeaderSize);
  return SkData::MakeWithCopy(agtm_payload_span.data(),
                              agtm_payload_span.size());
}

}  // namespace media
