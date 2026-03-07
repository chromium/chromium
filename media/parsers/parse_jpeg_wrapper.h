// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_PARSERS_PARSE_JPEG_WRAPPER_H_
#define MEDIA_PARSERS_PARSE_JPEG_WRAPPER_H_

#include <stdint.h>

#include "base/containers/span.h"
#include "media/base/media_export.h"
#include "media/parsers/jpeg_parser.h"

namespace media {

MEDIA_EXPORT bool ParseJpegPictureRust(base::span<const uint8_t> buffer,
                                       JpegParseResult* result);

}  // namespace media
#endif  // MEDIA_PARSERS_PARSE_JPEG_WRAPPER_H_
