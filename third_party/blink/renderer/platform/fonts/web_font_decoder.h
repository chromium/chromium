/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_WEB_FONT_DECODER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_WEB_FONT_DECODER_H_

#include <memory>

#include "base/types/expected.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkTypeface;

namespace blink {

class IftPatcher;
class SegmentedBuffer;

// Represents a font that has been successfully decoded and sanitized.
//
// Decoding: Bytes are converted from woff/woff2 into standard SFNT format.
// Sanitizing: Cleans font data to prevent some exploits.
struct PLATFORM_EXPORT DecodedWebFont {
  // Used by Skia for rasterizing fonts.
  sk_sp<SkTypeface> sk_typeface;

  // The size of the font after decoding and sanitizing.
  size_t decoded_size = 0;

  // Used to discover and apply patches to fonts. `nullptr` if the font is not
  // IFT-encoded or if Incremental Font Transfer is disabled.
  std::unique_ptr<IftPatcher> ift_patcher;

  // Defined out of line so that `IftPatcher` only needs to be forward declared
  // here.
  DecodedWebFont();
  DecodedWebFont(DecodedWebFont&&) noexcept;
  DecodedWebFont& operator=(DecodedWebFont&&) noexcept;
  DecodedWebFont(const DecodedWebFont&) = delete;
  DecodedWebFont& operator=(const DecodedWebFont&) = delete;
  ~DecodedWebFont();
};

// Decodes, decompresses, and sanitizes the raw font data from the provided
// `buffer` (which typically contains WOFF, WOFF2, or TTF data).
//
// Returns:
// - On success: A `DecodedWebFont` containing the successfully created
//   font.
// - On failure: A `String` containing a descriptive error message (e.g., if
//   the buffer is empty, the decompressed size exceeds limits, or OTS
//   validation fails).
PLATFORM_EXPORT base::expected<DecodedWebFont, String> DecodeWebFont(
    SegmentedBuffer* buffer);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_WEB_FONT_DECODER_H_
