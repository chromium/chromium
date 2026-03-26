// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AGTM_H_
#define MEDIA_BASE_AGTM_H_

#include <optional>

#include "base/containers/span.h"
#include "media/base/media_export.h"
#include "ui/gfx/hdr_metadata.h"

namespace media {

// Returns true if `t35_prefix` matches the AGTM ITU-T T.35 codes exactly (with
// `t35_prefix` having no extra bytes).
MEDIA_EXPORT bool MatchesAgtmT35(base::span<const uint8_t> t35_prefix);

// If `t35_country_code` and `t35_payload_without_country_code` match the AGTM
// ITU-T T.35 codes, then return the sub-span of
// `t35_payload_without_country_code` for the AGTM metadata.
MEDIA_EXPORT std::optional<base::span<const uint8_t>>
GetAgtmFromT35WithCountryCode(
    uint8_t t35_country_code,
    base::span<const uint8_t> t35_payload_without_country_code);

// If `t35_payload` begins with the AGTM ITU-T T.35 codes, then return the
// sub-span of `t35_payload` for the AGTM metadata.
MEDIA_EXPORT std::optional<base::span<const uint8_t>> GetAgtmFromT35(
    base::span<const uint8_t> t35_payload);

}  // namespace media

#endif  // MEDIA_BASE_AGTM_H_
