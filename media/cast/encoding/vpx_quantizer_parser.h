// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_ENCODING_VPX_QUANTIZER_PARSER_H_
#define MEDIA_CAST_ENCODING_VPX_QUANTIZER_PARSER_H_

#include <stdint.h>

#include <optional>

#include "base/containers/span.h"

namespace media::cast {

// Partially parse / skip data in the header and the first partition, and return
// the base quantizer in the range [0,63], or std::nullopt on parse error.
std::optional<int> ParseVpxHeaderQuantizer(base::span<const uint8_t> data);

}  // namespace media::cast

#endif  // MEDIA_CAST_ENCODING_VPX_QUANTIZER_PARSER_H_
