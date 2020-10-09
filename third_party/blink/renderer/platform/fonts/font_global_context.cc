// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_global_context.h"

#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_unique_name_lookup.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_font_cache.h"
#include "third_party/blink/renderer/platform/privacy_budget/identifiability_digest_helpers.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"

// While the size of these caches should usually be small (up to tens), we
// protect against the possibility of it growing quickly to thousands when
// animating variable font parameters.
static constexpr size_t kCachesMaxSize = 250;

namespace blink {

FontGlobalContext* FontGlobalContext::Get(CreateIfNeeded create_if_needed) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<FontGlobalContext*>,
                                  font_persistent, ());
  if (!*font_persistent && create_if_needed == kCreate) {
    *font_persistent = new FontGlobalContext();
  }
  return *font_persistent;
}

FontGlobalContext::FontGlobalContext()
    : harfbuzz_font_funcs_skia_advances_(nullptr),
      harfbuzz_font_funcs_harfbuzz_advances_(nullptr),
      typeface_digest_cache_(kCachesMaxSize),
      postscript_name_digest_cache_(kCachesMaxSize) {}

FontGlobalContext::~FontGlobalContext() = default;

FontUniqueNameLookup* FontGlobalContext::GetFontUniqueNameLookup() {
  if (!Get()->font_unique_name_lookup_) {
    Get()->font_unique_name_lookup_ =
        FontUniqueNameLookup::GetPlatformUniqueNameLookup();
  }
  return Get()->font_unique_name_lookup_.get();
}

HarfBuzzFontCache* FontGlobalContext::GetHarfBuzzFontCache() {
  std::unique_ptr<HarfBuzzFontCache>& global_context_harfbuzz_font_cache =
      Get()->harfbuzz_font_cache_;
  if (!global_context_harfbuzz_font_cache) {
    global_context_harfbuzz_font_cache = std::make_unique<HarfBuzzFontCache>();
  }
  return global_context_harfbuzz_font_cache.get();
}

IdentifiableToken FontGlobalContext::GetOrComputeTypefaceDigest(
    const FontPlatformData& source) {
  SkTypeface* typeface = source.Typeface();
  if (!typeface)
    return 0;

  SkFontID font_id = typeface->uniqueID();

  IdentifiableToken* cached_value = typeface_digest_cache_.Get(font_id);
  if (!cached_value) {
    typeface_digest_cache_.Put(font_id, source.ComputeTypefaceDigest());
    cached_value = typeface_digest_cache_.Get(font_id);
  } else {
    DCHECK(*cached_value == source.ComputeTypefaceDigest());
  }
  return *cached_value;
}

IdentifiableToken FontGlobalContext::GetOrComputePostScriptNameDigest(
    const FontPlatformData& source) {
  SkTypeface* typeface = source.Typeface();
  if (!typeface)
    return IdentifiableToken();

  SkFontID font_id = typeface->uniqueID();

  IdentifiableToken* cached_value = postscript_name_digest_cache_.Get(font_id);
  if (!cached_value) {
    postscript_name_digest_cache_.Put(
        font_id, IdentifiabilityBenignStringToken(source.GetPostScriptName()));
    cached_value = postscript_name_digest_cache_.Get(font_id);
  } else {
    DCHECK(*cached_value ==
           IdentifiabilityBenignStringToken(source.GetPostScriptName()));
  }
  return *cached_value;
}

void FontGlobalContext::ClearMemory() {
  if (!Get(kDoNotCreate))
    return;

  GetFontCache().Invalidate();
  Get()->typeface_digest_cache_.Clear();
  Get()->postscript_name_digest_cache_.Clear();
}

}  // namespace blink
