// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_font_cache.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_font_data.h"

namespace blink {

HbFontCacheEntry::HbFontCacheEntry(hb_font_t* font)
    : hb_font_(HbFontUniquePtr(font)),
      hb_font_data_(std::make_unique<HarfBuzzFontData>()) {}

HbFontCacheEntry::~HbFontCacheEntry() = default;

scoped_refptr<HbFontCacheEntry> HbFontCacheEntry::Create(hb_font_t* hb_font) {
  DCHECK(hb_font);
  return base::AdoptRef(new HbFontCacheEntry(hb_font));
}
}  // namespace blink
