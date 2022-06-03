// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_HARFBUZZ_FONT_CACHE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_HARFBUZZ_FONT_CACHE_H_

#include <hb.h>

#include <memory>

#include "third_party/blink/renderer/platform/fonts/font_metrics.h"
#include "third_party/blink/renderer/platform/fonts/unicode_range_set.h"
#include "third_party/harfbuzz-ng/utils/hb_scoped.h"

namespace blink {

struct HarfBuzzFontData;

// Though we have FontCache class, which provides the cache mechanism for
// WebKit's font objects, we also need additional caching layer for HarfBuzz to
// reduce the number of hb_font_t objects created. Without it, we would create
// an hb_font_t object for every FontPlatformData object. But insted, we only
// need one for each unique SkTypeface.
// FIXME, crbug.com/609099: We should fix the FontCache to only keep one
// FontPlatformData object independent of size, then consider using this here.
class HbFontCacheEntry : public RefCounted<HbFontCacheEntry> {
  USING_FAST_MALLOC(HbFontCacheEntry);

 public:
  static scoped_refptr<HbFontCacheEntry> Create(hb_font_t* hb_font);

  hb_font_t* HbFont() { return hb_font_.get(); }
  HarfBuzzFontData* HbFontData() { return hb_font_data_.get(); }

  ~HbFontCacheEntry();

 private:
  explicit HbFontCacheEntry(hb_font_t* font);

  HbScoped<hb_font_t> hb_font_;
  std::unique_ptr<HarfBuzzFontData> hb_font_data_;
};

// Declare as derived class in order to be able to forward-declare it as class
// in FontGlobalContext.
class HarfBuzzFontCache
    : public HashMap<uint64_t,
                     scoped_refptr<HbFontCacheEntry>,
                     WTF::IntHash<uint64_t>,
                     WTF::UnsignedWithZeroKeyHashTraits<uint64_t>> {};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_HARFBUZZ_FONT_CACHE_H_
