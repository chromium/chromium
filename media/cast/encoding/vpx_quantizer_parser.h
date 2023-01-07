// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_ENCODING_VPX_QUANTIZER_PARSER_H_
#define MEDIA_CAST_ENCODING_VPX_QUANTIZER_PARSER_H_

#include <stddef.h>
#include <stdint.h>

#include "media/cast/cast_config.h"

namespace media {
namespace cast {

// Partially parse / skip data in the header and the first partition,
// and return the base quantizer in the range [0,63], or -1 on parse error.
int ParseVpxHeaderQuantizer(const uint8_t* data, size_t size);

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_ENCODING_VPX_QUANTIZER_PARSER_H_
