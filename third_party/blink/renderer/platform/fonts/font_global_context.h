// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_GLOBAL_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_GLOBAL_CONTEXT_H_

#include "third_party/blink/public/common/privacy_budget/identifiable_token.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/text/layout_locale.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/lru_cache.h"
#include "third_party/skia/include/core/SkTypeface.h"

struct hb_font_funcs_t;

namespace blink {

class FontCache;
class FontUniqueNameLookup;
class HarfBuzzFontCache;

enum CreateIfNeeded { kDoNotCreate, kCreate };

// FontGlobalContext contains non-thread-safe, thread-specific data used for
// font formatting.
class PLATFORM_EXPORT FontGlobalContext {
  USING_FAST_MALLOC(FontGlobalContext);

 public:
  static FontGlobalContext* Get(CreateIfNeeded = kCreate);

  static inline FontCache& GetFontCache() { return Get()->font_cache_; }

  static HarfBuzzFontCache* GetHarfBuzzFontCache();

  enum HorizontalAdvanceSource {
    kSkiaHorizontalAdvances,
    kHarfBuzzHorizontalAdvances
  };

  static hb_font_funcs_t* GetHarfBuzzFontFuncs(
      HorizontalAdvanceSource advance_source) {
    if (advance_source == kHarfBuzzHorizontalAdvances) {
      return Get()->harfbuzz_font_funcs_harfbuzz_advances_;
    }
    return Get()->harfbuzz_font_funcs_skia_advances_;
  }

  static void SetHarfBuzzFontFuncs(HorizontalAdvanceSource advance_source,
                                   hb_font_funcs_t* funcs) {
    if (advance_source == kHarfBuzzHorizontalAdvances) {
      Get()->harfbuzz_font_funcs_harfbuzz_advances_ = funcs;
    }
    Get()->harfbuzz_font_funcs_skia_advances_ = funcs;
  }

  static FontUniqueNameLookup* GetFontUniqueNameLookup();

  IdentifiableToken GetOrComputeTypefaceDigest(const FontPlatformData& source);

  // Called by MemoryPressureListenerRegistry to clear memory.
  static void ClearMemory();

 private:
  friend class WTF::ThreadSpecific<FontGlobalContext>;

  FontGlobalContext();
  ~FontGlobalContext();

  FontCache font_cache_;
  std::unique_ptr<HarfBuzzFontCache> harfbuzz_font_cache_;
  hb_font_funcs_t* harfbuzz_font_funcs_skia_advances_;
  hb_font_funcs_t* harfbuzz_font_funcs_harfbuzz_advances_;
  std::unique_ptr<FontUniqueNameLookup> font_unique_name_lookup_;
  WTF::LruCache<SkFontID, IdentifiableToken> typeface_digest_cache_;

  DISALLOW_COPY_AND_ASSIGN(FontGlobalContext);
};

}  // namespace blink

#endif
