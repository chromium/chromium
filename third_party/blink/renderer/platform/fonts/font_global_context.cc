// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_global_context.h"

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_unique_name_lookup.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_face.h"
#include "third_party/blink/renderer/platform/privacy_budget/identifiability_digest_helpers.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"

// While the size of these caches should usually be small (up to tens), we
// protect against the possibility of it growing quickly to thousands when
// animating variable font parameters.
static constexpr size_t kCachesMaxSize = 250;

namespace blink {

ThreadSpecific<Persistent<FontGlobalContext>>&
GetThreadSpecificFontGlobalContextPool() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<Persistent<FontGlobalContext>>,
                                  thread_specific_pool, ());
  return thread_specific_pool;
}

FontGlobalContext& FontGlobalContext::Get() {
  auto& thread_specific_pool = GetThreadSpecificFontGlobalContextPool();
  if (!*thread_specific_pool)
    *thread_specific_pool = MakeGarbageCollected<FontGlobalContext>(PassKey());
  return **thread_specific_pool;
}

FontGlobalContext* FontGlobalContext::TryGet() {
  return GetThreadSpecificFontGlobalContextPool()->Get();
}

FontGlobalContext::FontGlobalContext(PassKey)
    : typeface_digest_cache_(kCachesMaxSize),
      postscript_name_digest_cache_(kCachesMaxSize) {}

FontGlobalContext::~FontGlobalContext() = default;

FontUniqueNameLookup* FontGlobalContext::GetFontUniqueNameLookup() {
  if (!Get().font_unique_name_lookup_) {
    Get().font_unique_name_lookup_ =
        FontUniqueNameLookup::GetPlatformUniqueNameLookup();
  }
  return Get().font_unique_name_lookup_.get();
}

IdentifiableToken FontGlobalContext::GetOrComputeTypefaceDigest(
    const FontPlatformData& source) {
  SkTypeface* typeface = source.Typeface();
  if (!typeface)
    return 0;

  SkTypefaceID font_id = typeface->uniqueID();

  auto iter = typeface_digest_cache_.Get(font_id);
  if (iter == typeface_digest_cache_.end())
    iter = typeface_digest_cache_.Put(font_id, source.ComputeTypefaceDigest());
  DCHECK(iter->second == source.ComputeTypefaceDigest());
  return iter->second;
}

IdentifiableToken FontGlobalContext::GetOrComputePostScriptNameDigest(
    const FontPlatformData& source) {
  SkTypeface* typeface = source.Typeface();
  if (!typeface)
    return IdentifiableToken();

  SkTypefaceID font_id = typeface->uniqueID();

  auto iter = postscript_name_digest_cache_.Get(font_id);
  if (iter == postscript_name_digest_cache_.end())
    iter = postscript_name_digest_cache_.Put(
        font_id, IdentifiabilityBenignStringToken(source.GetPostScriptName()));
  DCHECK(iter->second ==
         IdentifiabilityBenignStringToken(source.GetPostScriptName()));
  return iter->second;
}

void FontGlobalContext::ClearMemory() {
  FontGlobalContext* const context = TryGet();
  if (!context)
    return;

  context->font_cache_.Invalidate();
  context->typeface_digest_cache_.Clear();
  context->postscript_name_digest_cache_.Clear();
}

void FontGlobalContext::Init() {
  DCHECK(IsMainThread());
  if (auto* name_lookup = FontGlobalContext::Get().GetFontUniqueNameLookup())
    name_lookup->Init();
  HarfBuzzFace::Init();
}

}  // namespace blink
