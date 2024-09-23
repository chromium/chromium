// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_PARSERS_WEBP_PARSER_H_
#define MEDIA_PARSERS_WEBP_PARSER_H_

#include <stdint.h>

#include <memory>

#include "base/containers/span.h"
#include "media/base/media_export.h"

namespace media {

struct Vp8FrameHeader;

// A lightweight WebP file header parser to extract feature and size
// information. It validates that a given data stream encodes a simple lossy
// WebP image and populates a Vp8FrameHeader upon successful parsing.
// For more information, see the WebP Container Specification:
// https://developers.google.com/speed/webp/docs/riff_container

// Returns true if |encoded_data| claims to encode a simple (non-extended) lossy
// WebP image. Returns false otherwise.
MEDIA_EXPORT
bool IsLossyWebPImage(base::span<const uint8_t> encoded_data);

// Parses a simple (non-extended) lossy WebP image and returns a Vp8FrameHeader
// containing the parsed VP8 frame contained by the image. Returns nullptr on
// failure.
MEDIA_EXPORT
std::unique_ptr<Vp8FrameHeader> ParseWebPImage(
    base::span<const uint8_t> encoded_data);

}  // namespace media

#endif  // MEDIA_PARSERS_WEBP_PARSER_H_
