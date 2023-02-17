// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_font_cache.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_face.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_font_data.h"

namespace blink {

HbFontCacheEntry::HbFontCacheEntry(hb_font_t* font)
    : hb_font_(hb::unique_ptr<hb_font_t>(font)),
      hb_font_data_(std::make_unique<HarfBuzzFontData>()) {}

HbFontCacheEntry::~HbFontCacheEntry() = default;

scoped_refptr<HbFontCacheEntry> HbFontCacheEntry::Create(hb_font_t* hb_font) {
  DCHECK(hb_font);
  return base::AdoptRef(new HbFontCacheEntry(hb_font));
}

HarfBuzzFontCache::HarfBuzzFontCache() = default;
HarfBuzzFontCache::~HarfBuzzFontCache() = default;

// See "harfbuzz_face.cc" for |HarfBuzzFontCache::GetOrCreateFontData()|
// implementation.

void HarfBuzzFontCache::Remove(uint64_t unique_id) {
  auto it = font_map_.find(unique_id);
  // TODO(https://crbug.com/1417160): In tests such as FontObjectThreadedTest
  // that test taking down FontGlobalContext an object may not be found due to
  // existing issues with refcounting of font objects at thread destruction
  // time.
  if (it == font_map_.end()) {
    return;
  }
  DCHECK(!it.Get()->value->HasOneRef());
  it.Get()->value->Release();
  if (!it.Get()->value->HasOneRef()) {
    return;
  }
  font_map_.erase(it);
}

}  // namespace blink
