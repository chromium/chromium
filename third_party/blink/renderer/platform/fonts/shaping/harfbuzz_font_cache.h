// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_HARFBUZZ_FONT_CACHE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_HARFBUZZ_FONT_CACHE_H_

#include <memory>

#include "third_party/blink/renderer/platform/fonts/font_metrics.h"
#include "third_party/blink/renderer/platform/fonts/unicode_range_set.h"

struct hb_font_t;
struct hb_face_t;

namespace blink {

struct HarfBuzzFontData;

struct HbFontDeleter {
  void operator()(hb_font_t* font);
};

using HbFontUniquePtr = std::unique_ptr<hb_font_t, HbFontDeleter>;

struct HbFaceDeleter {
  void operator()(hb_face_t* face);
};

using HbFaceUniquePtr = std::unique_ptr<hb_face_t, HbFaceDeleter>;

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

  HbFontUniquePtr hb_font_;
  std::unique_ptr<HarfBuzzFontData> hb_font_data_;
};

typedef HashMap<uint64_t,
                scoped_refptr<HbFontCacheEntry>,
                WTF::IntHash<uint64_t>,
                WTF::UnsignedWithZeroKeyHashTraits<uint64_t>>
    HarfBuzzFontCache;

}  // namespace blink

#endif
