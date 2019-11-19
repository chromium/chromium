/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Nicholas Shanks <webkit@nickshanks.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/fonts/font_cache.h"

#include <limits>
#include <memory>

#include "base/debug/alias.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/font_family_names.h"
#include "third_party/blink/renderer/platform/fonts/alternate_font_family.h"
#include "third_party/blink/renderer/platform/fonts/font_cache_client.h"
#include "third_party/blink/renderer/platform/fonts/font_cache_key.h"
#include "third_party/blink/renderer/platform/fonts/font_data_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_global_context.h"
#include "third_party/blink/renderer/platform/fonts/font_platform_data.h"
#include "third_party/blink/renderer/platform/fonts/font_smoothing_mode.h"
#include "third_party/blink/renderer/platform/fonts/font_unique_name_lookup.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_cache.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/fonts/text_rendering_mode.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/web_memory_allocator_dump.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/web_process_memory_dump.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/layout_locale.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/gfx/font_list.h"

#if defined(OS_WIN)
#include "third_party/skia/include/ports/SkTypeface_win.h"
#endif

namespace blink {

// Special locale for retrieving the color emoji font based on the proposed
// changes in UTR #51 for introducing an Emoji script code:
// https://unicode.org/reports/tr51/#Emoji_Script
static const char kColorEmojiLocale[] = "und-Zsye";

SkFontMgr* FontCache::static_font_manager_ = nullptr;

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
float FontCache::device_scale_factor_ = 1.0;
#endif

#if defined(OS_WIN)
bool FontCache::antialiased_text_enabled_ = false;
bool FontCache::lcd_text_enabled_ = false;
bool FontCache::use_skia_font_fallback_ = false;
#endif  // defined(OS_WIN)

FontCache* FontCache::GetFontCache() {
  return &FontGlobalContext::GetFontCache();
}

FontCache::FontCache()
    : purge_prevent_count_(0),
      font_manager_(sk_ref_sp(static_font_manager_)),
      font_size_limit_(std::nextafter(
          (static_cast<float>(std::numeric_limits<unsigned>::max()) - 2.f) /
              static_cast<float>(blink::FontCacheKey::PrecisionMultiplier()),
          0.f)) {
#if defined(OS_WIN)
  if (!font_manager_) {
    // This code path is only for unit tests. This SkFontMgr does not work in
    // sandboxed environments, but injecting this initialization code to all
    // unit tests isn't easy.
    font_manager_ = SkFontMgr_New_DirectWrite();
    // Set |is_test_font_mgr_| to capture if this is not happening in the
    // production code. crbug.com/561873
    is_test_font_mgr_ = true;
  }
  DCHECK(font_manager_.get());
#endif
}

#if !defined(OS_MACOSX)
FontPlatformData* FontCache::SystemFontPlatformData(
    const FontDescription& font_description) {
  const AtomicString& family = FontCache::SystemFontFamily();
#if defined(OS_LINUX) || defined(OS_FUCHSIA)
  if (family.IsEmpty() || family == font_family_names::kSystemUi)
    return nullptr;
#else
  DCHECK(!family.IsEmpty() && family != font_family_names::kSystemUi);
#endif
  return GetFontPlatformData(font_description, FontFaceCreationParams(family),
                             AlternateFontName::kNoAlternate);
}
#endif

FontPlatformData* FontCache::GetFontPlatformData(
    const FontDescription& font_description,
    const FontFaceCreationParams& creation_params,
    AlternateFontName alternate_font_name) {
  TRACE_EVENT0("fonts", "FontCache::GetFontPlatformData");

  if (!platform_init_) {
    platform_init_ = true;
    PlatformInit();
  }

#if !defined(OS_MACOSX)
  if (creation_params.CreationType() == kCreateFontByFamily &&
      creation_params.Family() == font_family_names::kSystemUi) {
    return SystemFontPlatformData(font_description);
  }
#endif

  float size = font_description.EffectiveFontSize();
  size = std::min(size, font_size_limit_);

  unsigned rounded_size = size * FontCacheKey::PrecisionMultiplier();

  // Assert that the computed hash map key rounded_size value does not hit
  // the empty (max()) or deleted (max()-1) sentinel values of the hash map,
  // compare UnsignedWithZeroKeyHashTraits() in hash_traits.h.
  DCHECK_LT(rounded_size, std::numeric_limits<unsigned>::max() - 1);
  // Assert that rounded_size was not reset to 0 due to an integer overflow,
  // i.e. if size was non-zero, rounded_size can't be zero, but if size was 0,
  // it may be 0.
  DCHECK_EQ(!!size, !!rounded_size);

  bool is_unique_match =
      alternate_font_name == AlternateFontName::kLocalUniqueFace;
  FontCacheKey key =
      font_description.CacheKey(creation_params, is_unique_match);
  DCHECK(!key.IsHashTableDeletedValue());

  // Remove the font size from the cache key, and handle the font size
  // separately in the inner HashMap. So that different size of FontPlatformData
  // can share underlying SkTypeface.
  FontPlatformData* result;
  bool found_result;

  {
    // addResult's scope must end before we recurse for alternate family names
    // below, to avoid triggering its dtor hash-changed asserts.
    SizedFontPlatformDataSet* sized_fonts =
        &font_platform_data_cache_.insert(key, SizedFontPlatformDataSet())
             .stored_value->value;
    bool was_empty = sized_fonts->IsEmpty();

    // Take a different size instance of the same font before adding an entry to
    // |sizedFont|.
    FontPlatformData* another_size =
        was_empty ? nullptr : sized_fonts->begin()->value.get();
    auto add_result = sized_fonts->insert(rounded_size, nullptr);
    std::unique_ptr<FontPlatformData>* found = &add_result.stored_value->value;
    if (add_result.is_new_entry) {
      if (was_empty) {
        *found = CreateFontPlatformData(font_description, creation_params, size,
                                        alternate_font_name);
      } else if (another_size) {
        *found = ScaleFontPlatformData(*another_size, font_description,
                                       creation_params, size);
      }
    }

    result = found->get();
    found_result = result || !add_result.is_new_entry;
  }

  if (!found_result &&
      alternate_font_name == AlternateFontName::kAllowAlternate &&
      creation_params.CreationType() == kCreateFontByFamily) {
    // We were unable to find a font. We have a small set of fonts that we alias
    // to other names, e.g., Arial/Helvetica, Courier/Courier New, etc. Try
    // looking up the font under the aliased name.
    const AtomicString& alternate_name =
        AlternateFamilyName(creation_params.Family());
    if (!alternate_name.IsEmpty()) {
      FontFaceCreationParams create_by_alternate_family(alternate_name);
      result = GetFontPlatformData(font_description, create_by_alternate_family,
                                   AlternateFontName::kNoAlternate);
    }
    if (result) {
      // Cache the result under the old name.
      auto* adding =
          &font_platform_data_cache_.insert(key, SizedFontPlatformDataSet())
               .stored_value->value;
      adding->Set(rounded_size, std::make_unique<FontPlatformData>(*result));
    }
  }

  return result;
}

std::unique_ptr<FontPlatformData> FontCache::ScaleFontPlatformData(
    const FontPlatformData& font_platform_data,
    const FontDescription& font_description,
    const FontFaceCreationParams& creation_params,
    float font_size) {
  TRACE_EVENT0("fonts,ui", "FontCache::ScaleFontPlatformData");

#if defined(OS_MACOSX)
  return CreateFontPlatformData(font_description, creation_params, font_size);
#else
  return std::make_unique<FontPlatformData>(font_platform_data, font_size);
#endif
}

ShapeCache* FontCache::GetShapeCache(const FallbackListCompositeKey& key) {
  FallbackListShaperCache::iterator it = fallback_list_shaper_cache_.find(key);
  ShapeCache* result = nullptr;
  if (it == fallback_list_shaper_cache_.end()) {
    result = new ShapeCache();
    fallback_list_shaper_cache_.Set(key, base::WrapUnique(result));
  } else {
    result = it->value.get();
  }

  DCHECK(result);
  return result;
}

void FontCache::SetFontManager(sk_sp<SkFontMgr> font_manager) {
  DCHECK(!static_font_manager_);
  static_font_manager_ = font_manager.release();
}

void FontCache::AcceptLanguagesChanged(const String& accept_languages) {
  LayoutLocale::AcceptLanguagesChanged(accept_languages);
  GetFontCache()->InvalidateShapeCache();
}

scoped_refptr<SimpleFontData> FontCache::GetFontData(
    const FontDescription& font_description,
    const AtomicString& family,
    AlternateFontName altername_font_name,
    ShouldRetain should_retain) {
  if (FontPlatformData* platform_data = GetFontPlatformData(
          font_description,
          FontFaceCreationParams(
              AdjustFamilyNameToAvoidUnsupportedFonts(family)),
          altername_font_name)) {
    return FontDataFromFontPlatformData(
        platform_data, should_retain, font_description.SubpixelAscentDescent());
  }

  return nullptr;
}

scoped_refptr<SimpleFontData> FontCache::FontDataFromFontPlatformData(
    const FontPlatformData* platform_data,
    ShouldRetain should_retain,
    bool subpixel_ascent_descent) {
#if DCHECK_IS_ON()
  if (should_retain == kDoNotRetain)
    DCHECK(purge_prevent_count_);
#endif

  return font_data_cache_.Get(platform_data, should_retain,
                              subpixel_ascent_descent);
}

bool FontCache::IsPlatformFamilyMatchAvailable(
    const FontDescription& font_description,
    const AtomicString& family) {
  return GetFontPlatformData(
      font_description,
      FontFaceCreationParams(AdjustFamilyNameToAvoidUnsupportedFonts(family)),
      AlternateFontName::kNoAlternate);
}

bool FontCache::IsPlatformFontUniqueNameMatchAvailable(
    const FontDescription& font_description,
    const AtomicString& unique_font_name) {
  return GetFontPlatformData(font_description,
                             FontFaceCreationParams(unique_font_name),
                             AlternateFontName::kLocalUniqueFace);
}

String FontCache::FirstAvailableOrFirst(const String& families) {
  // The conversions involve at least two string copies, and more if non-ASCII.
  // For now we prefer shared code over the cost because a) inputs are
  // only from grd/xtb and all ASCII, and b) at most only a few times per
  // setting change/script.
  return String::FromUTF8(
      gfx::FontList::FirstAvailableOrFirst(families.Utf8().c_str()));
}

SimpleFontData* FontCache::GetNonRetainedLastResortFallbackFont(
    const FontDescription& font_description) {
  auto font = GetLastResortFallbackFont(font_description, kDoNotRetain);
  if (font)
    font->AddRef();
  return font.get();
}

scoped_refptr<SimpleFontData> FontCache::FallbackFontForCharacter(
    const FontDescription& description,
    UChar32 lookup_char,
    const SimpleFontData* font_data_to_substitute,
    FontFallbackPriority fallback_priority) {
  TRACE_EVENT0("fonts", "FontCache::FallbackFontForCharacter");

  // In addition to PUA, do not perform fallback for non-characters either. Some
  // of these are sentinel characters to detect encodings and do appear on
  // websites. More details on
  // http://www.unicode.org/faq/private_use.html#nonchar1 - See also
  // crbug.com/862352 where performing fallback for U+FFFE causes a memory
  // regression.
  if (Character::IsPrivateUse(lookup_char) ||
      Character::IsNonCharacter(lookup_char))
    return nullptr;
  return PlatformFallbackFontForCharacter(
      description, lookup_char, font_data_to_substitute, fallback_priority);
}

void FontCache::ReleaseFontData(const SimpleFontData* font_data) {
  font_data_cache_.Release(font_data);
}

void FontCache::PurgePlatformFontDataCache() {
  TRACE_EVENT0("fonts,ui", "FontCache::PurgePlatformFontDataCache");
  Vector<FontCacheKey> keys_to_remove;
  keys_to_remove.ReserveInitialCapacity(font_platform_data_cache_.size());
  for (auto& sized_fonts : font_platform_data_cache_) {
    Vector<unsigned> sizes_to_remove;
    sizes_to_remove.ReserveInitialCapacity(sized_fonts.value.size());
    for (const auto& platform_data : sized_fonts.value) {
      if (platform_data.value &&
          !font_data_cache_.Contains(platform_data.value.get()))
        sizes_to_remove.push_back(platform_data.key);
    }
    sized_fonts.value.RemoveAll(sizes_to_remove);
    if (sized_fonts.value.IsEmpty())
      keys_to_remove.push_back(sized_fonts.key);
  }
  font_platform_data_cache_.RemoveAll(keys_to_remove);
}

void FontCache::PurgeFallbackListShaperCache() {
  TRACE_EVENT0("fonts,ui", "FontCache::PurgeFallbackListShaperCache");
  unsigned items = 0;
  FallbackListShaperCache::iterator iter;
  for (iter = fallback_list_shaper_cache_.begin();
       iter != fallback_list_shaper_cache_.end(); ++iter) {
    items += iter->value->size();
  }
  fallback_list_shaper_cache_.clear();
  DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, shape_cache_histogram,
                                  ("Blink.Fonts.ShapeCache", 1, 1000000, 50));
  shape_cache_histogram.Count(items);
}

void FontCache::InvalidateShapeCache() {
  PurgeFallbackListShaperCache();
}

void FontCache::Purge(PurgeSeverity purge_severity) {
  // Ideally we should never be forcing the purge while the
  // FontCachePurgePreventer is in scope, but we call purge() at any timing
  // via MemoryPressureListenerRegistry.
  if (purge_prevent_count_)
    return;

  if (!font_data_cache_.Purge(purge_severity))
    return;

  PurgePlatformFontDataCache();
  PurgeFallbackListShaperCache();
}

void FontCache::AddClient(FontCacheClient* client) {
  CHECK(client);
  if (!font_cache_clients_) {
    font_cache_clients_ =
        MakeGarbageCollected<HeapHashSet<WeakMember<FontCacheClient>>>();
    font_cache_clients_.RegisterAsStaticReference();
  }
  DCHECK(!font_cache_clients_->Contains(client));
  font_cache_clients_->insert(client);
}

uint16_t FontCache::Generation() {
  return generation_;
}

void FontCache::Invalidate() {
  TRACE_EVENT0("fonts,ui", "FontCache::Invalidate");
  font_platform_data_cache_.clear();
  generation_++;

  if (font_cache_clients_) {
    for (const auto& client : *font_cache_clients_)
      client->FontCacheInvalidated();
  }

  Purge(kForcePurge);
}

void FontCache::CrashWithFontInfo(const FontDescription* font_description) {
  FontCache* font_cache = nullptr;
  SkFontMgr* font_mgr = nullptr;
  int num_families = std::numeric_limits<int>::min();
  bool is_test_font_mgr = false;
  if (FontGlobalContext::Get(kDoNotCreate)) {
    font_cache = FontCache::GetFontCache();
    if (font_cache) {
#if defined(OS_WIN)
      is_test_font_mgr = font_cache->is_test_font_mgr_;
#endif
      font_mgr = font_cache->font_manager_.get();
      if (font_mgr)
        num_families = font_mgr->countFamilies();
    }
  }

  // In production, these 3 font managers must match.
  // They don't match in unit tests or in single process mode.
  SkFontMgr* static_font_mgr = static_font_manager_;
  SkFontMgr* skia_default_font_mgr = SkFontMgr::RefDefault().get();
  base::debug::Alias(&font_mgr);
  base::debug::Alias(&static_font_mgr);
  base::debug::Alias(&skia_default_font_mgr);

  FontDescription font_description_copy = *font_description;
  base::debug::Alias(&font_description_copy);
  base::debug::Alias(&is_test_font_mgr);
  base::debug::Alias(&num_families);

  CHECK(false);
}

void FontCache::DumpFontPlatformDataCache(
    base::trace_event::ProcessMemoryDump* memory_dump) {
  DCHECK(IsMainThread());
  base::trace_event::MemoryAllocatorDump* dump =
      memory_dump->CreateAllocatorDump("font_caches/font_platform_data_cache");
  size_t font_platform_data_objects_size =
      font_platform_data_cache_.size() * sizeof(FontPlatformData);
  dump->AddScalar("size", "bytes", font_platform_data_objects_size);
  memory_dump->AddSuballocation(dump->guid(),
                                WTF::Partitions::kAllocatedObjectPoolName);
}

void FontCache::DumpShapeResultCache(
    base::trace_event::ProcessMemoryDump* memory_dump) {
  DCHECK(IsMainThread());
  base::trace_event::MemoryAllocatorDump* dump =
      memory_dump->CreateAllocatorDump("font_caches/shape_caches");
  size_t shape_result_cache_size = 0;
  FallbackListShaperCache::iterator iter;
  for (iter = fallback_list_shaper_cache_.begin();
       iter != fallback_list_shaper_cache_.end(); ++iter) {
    shape_result_cache_size += iter->value->ByteSize();
  }
  dump->AddScalar("size", "bytes", shape_result_cache_size);
  memory_dump->AddSuballocation(dump->guid(),
                                WTF::Partitions::kAllocatedObjectPoolName);
}

sk_sp<SkTypeface> FontCache::CreateTypefaceFromUniqueName(
    const FontFaceCreationParams& creation_params) {
  FontUniqueNameLookup* unique_name_lookup =
      FontGlobalContext::Get()->GetFontUniqueNameLookup();
  DCHECK(unique_name_lookup);
  sk_sp<SkTypeface> uniquely_identified_font =
      unique_name_lookup->MatchUniqueName(creation_params.Family());
  if (uniquely_identified_font) {
    return uniquely_identified_font;
  }
  return nullptr;
}

// static
FontCache::Bcp47Vector FontCache::GetBcp47LocaleForRequest(
    const FontDescription& font_description,
    FontFallbackPriority fallback_priority) {
  Bcp47Vector result;

  // Fill in the list of locales in the reverse priority order.
  // Skia expects the highest array index to be the first priority.
  const LayoutLocale* content_locale = font_description.Locale();
  if (const LayoutLocale* han_locale =
          LayoutLocale::LocaleForHan(content_locale)) {
    result.push_back(han_locale->LocaleForHanForSkFontMgr());
  }
  result.push_back(LayoutLocale::GetDefault().LocaleForSkFontMgr());
  if (content_locale)
    result.push_back(content_locale->LocaleForSkFontMgr());

  if (fallback_priority == FontFallbackPriority::kEmojiEmoji)
    result.push_back(kColorEmojiLocale);
  return result;
}

}  // namespace blink
