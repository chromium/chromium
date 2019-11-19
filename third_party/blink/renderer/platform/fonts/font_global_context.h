// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_GLOBAL_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_GLOBAL_CONTEXT_H_

#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_font_cache.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/text/layout_locale.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

struct hb_font_funcs_t;

namespace blink {

class FontCache;
class FontUniqueNameLookup;

enum CreateIfNeeded { kDoNotCreate, kCreate };

// FontGlobalContext contains non-thread-safe, thread-specific data used for
// font formatting.
class PLATFORM_EXPORT FontGlobalContext {
  USING_FAST_MALLOC(FontGlobalContext);

 public:
  static FontGlobalContext* Get(CreateIfNeeded = kCreate);

  static inline FontCache& GetFontCache() { return Get()->font_cache_; }

  static inline HarfBuzzFontCache& GetHarfBuzzFontCache() {
    return Get()->harfbuzz_font_cache_;
  }

  static hb_font_funcs_t* GetHarfBuzzFontFuncs() {
    return Get()->harfbuzz_font_funcs_;
  }

  static void SetHarfBuzzFontFuncs(hb_font_funcs_t* funcs) {
    Get()->harfbuzz_font_funcs_ = funcs;
  }

  static FontUniqueNameLookup* GetFontUniqueNameLookup();

  // Called by MemoryPressureListenerRegistry to clear memory.
  static void ClearMemory();

 private:
  friend class WTF::ThreadSpecific<FontGlobalContext>;

  FontGlobalContext();
  ~FontGlobalContext();

  FontCache font_cache_;
  HarfBuzzFontCache harfbuzz_font_cache_;
  hb_font_funcs_t* harfbuzz_font_funcs_;
  std::unique_ptr<FontUniqueNameLookup> font_unique_name_lookup_;

  DISALLOW_COPY_AND_ASSIGN(FontGlobalContext);
};

}  // namespace blink

#endif
