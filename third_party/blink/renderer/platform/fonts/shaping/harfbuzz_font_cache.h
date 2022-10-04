// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_HARFBUZZ_FONT_CACHE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_HARFBUZZ_FONT_CACHE_H_

#include "third_party/blink/renderer/platform/fonts/font_metrics.h"
#include "third_party/blink/renderer/platform/fonts/unicode_range_set.h"

namespace blink {

struct HarfBuzzFontData;

// Though we have FontCache class, which provides the cache mechanism for
// WebKit's font objects, we also need additional caching layer for HarfBuzz to
// reduce the number of hb_font_t objects created. Without it, we would create
// an hb_font_t object for every FontPlatformData object. But insted, we only
// need one for each unique SkTypeface.
// FIXME, crbug.com/609099: We should fix the FontCache to only keep one
// FontPlatformData object independent of size, then consider using this here.

// The HarfBuzzFontCache is thread specific cache for mapping
//  from |FontPlatformData| to |HarfBuzzFace|, and
//  from |FontPlatformData::UniqueID()| to |HarfBuzzFontData|.
//
//  |HarfBuzzFace| holds shared |HarfBuzzData| per unique id.
//
//  |FontPlatformData-1| |FontPlatformData-2|
//         |                    |
//    |HarfBuzzFace-1|     |HarfBuzzFace-2|
//         |                    |
//         +----------+---------+
//                    |
//               |HarfBuzzFontData|
//
class HarfBuzzFontCache final {
 public:
  HarfBuzzFontCache();
  ~HarfBuzzFontCache();

  scoped_refptr<HarfBuzzFontData> GetOrCreateFontData(
      FontPlatformData* platform_data);

 private:
  using FontDataMap = HashMap<uint64_t,
                              scoped_refptr<HarfBuzzFontData>,
                              WTF::IntHash<uint64_t>,
                              WTF::UnsignedWithZeroKeyHashTraits<uint64_t>>;

  FontDataMap font_map_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_HARFBUZZ_FONT_CACHE_H_
