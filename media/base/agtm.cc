// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/agtm.h"

#include "third_party/skia/include/core/SkData.h"

namespace media {

// Returns AGTM metadata if the ITU-T T.35 message contains some.
std::optional<gfx::HdrMetadataAgtm> GetHdrMetadataAgtmFromItutT35(
    uint8_t t35_country_code,
    base::span<const uint8_t> t35_payload) {
  // itu_t_t35_terminal_provider_code (2 bytes)
  // + itu_t_t35_terminal_provider_oriented_code (2 bytes)
  // + application_identifier (1 byte) + application_version (1 byte)
  static constexpr size_t kItuT35HeaderSize = 6;

  const bool isAgtm = t35_country_code == 0xB5 /* United States */ &&
                      t35_payload.size() >= kItuT35HeaderSize &&
                      t35_payload[0] == 0x58 /* placeholder (AOM) */ &&
                      t35_payload[1] == 0x90 /* placeholder (AOM) */ &&
                      t35_payload[2] == 0x69 /* placeholder (AGTM) */ &&
                      t35_payload[3] == 0x42 /* placeholder (AGTM) */ &&
                      t35_payload[4] == 0x05 /* app identifier */ &&
                      t35_payload[5] == 0x00 /* app version */;
  if (!isAgtm) {
    return std::nullopt;
  }
  const auto agtm_payload_span = t35_payload.subspan(kItuT35HeaderSize);
  sk_sp<SkData> agtm_skdata =
      SkData::MakeWithCopy(agtm_payload_span.data(), agtm_payload_span.size());
  return gfx::HdrMetadataAgtm(std::move(agtm_skdata));
}

}  // namespace media
