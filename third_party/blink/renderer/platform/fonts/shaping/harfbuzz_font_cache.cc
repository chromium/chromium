// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_font_cache.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_face.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_font_data.h"
#include "third_party/harfbuzz-ng/utils/hb_scoped.h"

namespace blink {

HbFontCacheEntry::HbFontCacheEntry(hb_font_t* font)
    : hb_font_(HbScoped<hb_font_t>(font)),
      hb_font_data_(std::make_unique<HarfBuzzFontData>()) {}

HbFontCacheEntry::~HbFontCacheEntry() = default;

scoped_refptr<HbFontCacheEntry> HbFontCacheEntry::Create(hb_font_t* hb_font) {
  DCHECK(hb_font);
  return base::AdoptRef(new HbFontCacheEntry(hb_font));
}

HarfBuzzFontCache::HarfBuzzFontCache() = default;
HarfBuzzFontCache::~HarfBuzzFontCache() = default;

// See "harfbuzz_face.cc" for HarfBuzzFontCache::GetOrNew() implementation

void HarfBuzzFontCache::Remove(uint64_t unique_id) {
  auto it = entries_.find(unique_id);
  SECURITY_DCHECK(it != entries_.end());
  DCHECK(!it.Get()->value->HasOneRef());
  it.Get()->value->Release();
  if (!it.Get()->value->HasOneRef())
    return;
  entries_.erase(it);
}

}  // namespace blink
