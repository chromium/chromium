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

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/fonts/typesetting_features.h"
#include "third_party/blink/renderer/platform/fonts/unicode_range_set.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/harfbuzz-ng/utils/hb_scoped.h"

#include <hb.h>

namespace blink {

class FontPlatformData;
struct HarfBuzzFontData;

class HarfBuzzFace : public RefCounted<HarfBuzzFace> {
 public:
  static scoped_refptr<HarfBuzzFace> Create(FontPlatformData* platform_data,
                                            uint64_t unique_id) {
    return base::AdoptRef(new HarfBuzzFace(platform_data, unique_id));
  }
  HarfBuzzFace(const HarfBuzzFace&) = delete;
  HarfBuzzFace& operator=(const HarfBuzzFace&) = delete;
  ~HarfBuzzFace();

  enum VerticalLayoutCallbacks { PrepareForVerticalLayout, NoVerticalLayout };

  // In order to support the restricting effect of unicode-range optionally a
  // range restriction can be passed in, which will restrict which glyphs we
  // return in the harfBuzzGetGlyph function.
  // Passing in specified_size in order to control selecting the right value
  // from the trak table. If not set, the size of the internal FontPlatformData
  // object will be used.
  hb_font_t* GetScaledFont(scoped_refptr<UnicodeRangeSet>,
                           VerticalLayoutCallbacks,
                           float specified_size = -1) const;

  bool HasSpaceInLigaturesOrKerning(TypesettingFeatures);
  unsigned UnitsPerEmFromHeadTable();

  bool ShouldSubpixelPosition();

 private:
  HarfBuzzFace(FontPlatformData*, uint64_t);

  HbScoped<hb_face_t> CreateFace();
  void PrepareHarfBuzzFontData();

  FontPlatformData* platform_data_;
  uint64_t unique_id_;
  hb_font_t* unscaled_font_;
  HarfBuzzFontData* harfbuzz_font_data_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_HARFBUZZ_FACE_H_
