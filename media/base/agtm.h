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

// Returns AGTM metadata if the ITU-T T.35 message contains some.
MEDIA_EXPORT std::optional<gfx::HdrMetadataAgtm> GetHdrMetadataAgtmFromItutT35(
    uint8_t t35_country_code,
    base::span<const uint8_t> t35_payload);

}  // namespace media

#endif  // MEDIA_BASE_AGTM_H_
