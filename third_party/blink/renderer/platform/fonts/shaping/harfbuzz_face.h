/*
 * Copyright (c) 2012 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_HARFBUZZ_FACE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_HARFBUZZ_FACE_H_

#include "third_party/blink/renderer/platform/fonts/glyph.h"
#include "third_party/blink/renderer/platform/fonts/typesetting_features.h"
#include "third_party/blink/renderer/platform/fonts/unicode_range_set.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

#include <hb.h>
#include <hb-cplusplus.hh>

namespace blink {

class FontPlatformData;
struct HarfBuzzFontData;

// |HarfBuzzFace| is a thread specific data associated to |FontPlatformData|,
// hold by |HarfBuzzFontCache|.
class HarfBuzzFace final : public RefCounted<HarfBuzzFace> {
  USING_FAST_MALLOC(HarfBuzzFace);

 public:
  static scoped_refptr<HarfBuzzFace> Create(FontPlatformData* platform_data,
                                            uint64_t unique_id) {
    return base::AdoptRef(new HarfBuzzFace(platform_data, unique_id));
  }

  HarfBuzzFace(const HarfBuzzFace&) = delete;
  HarfBuzzFace& operator=(const HarfBuzzFace&) = delete;
  ~HarfBuzzFace();

  enum VerticalLayoutCallbacks { kPrepareForVerticalLayout, kNoVerticalLayout };

  // In order to support the restricting effect of unicode-range optionally a
  // range restriction can be passed in, which will restrict which glyphs we
  // return in the harfBuzzGetGlyph function.
  // Passing in specified_size in order to control selecting the right value
  // from the trak table. If not set, the size of the internal FontPlatformData
  // object will be used.
  hb_font_t* GetScaledFont(scoped_refptr<UnicodeRangeSet>,
                           VerticalLayoutCallbacks,
                           float specified_size) const;

  // Returns `hb_font_t` as same as `GetScaledFont()` with null
  // `UnicodeRangeSet`, `HarfBuzzFace::kNoVerticalLayout`, and
  // `platform_data_.size()`.
  hb_font_t* GetScaledFont() const;

  bool HasSpaceInLigaturesOrKerning(TypesettingFeatures);
  unsigned UnitsPerEmFromHeadTable();
  Glyph HbGlyphForCharacter(UChar32 character);

  bool ShouldSubpixelPosition();

  static void Init();

 private:
  HarfBuzzFace(FontPlatformData* platform_data, uint64_t);

  void PrepareHarfBuzzFontData();

  FontPlatformData* const platform_data_;
  const uint64_t unique_id_;
  // TODO(crbug.com/1489080): When briefly given MiraclePtr protection,
  // these members were both found dangling.
  hb_font_t* unscaled_font_;
  HarfBuzzFontData* harfbuzz_font_data_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_HARFBUZZ_FACE_H_
