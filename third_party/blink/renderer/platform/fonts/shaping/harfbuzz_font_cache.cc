// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_font_cache.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_face.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_font_data.h"

namespace blink {

void HarfBuzzFontCache::Trace(Visitor* visitor) const {
  for (const auto& font_map : font_maps_) {
    visitor->Trace(font_map);
  }
}

}  // namespace blink
