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
#include "base/feature_list.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/font_family_names.h"
#include "third_party/blink/renderer/platform/fonts/alternate_font_family.h"
#include "third_party/blink/renderer/platform/fonts/font_cache_client.h"
#include "third_party/blink/renderer/platform/fonts/font_data_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_fallback_map.h"
#include "third_party/blink/renderer/platform/fonts/font_global_context.h"
#include "third_party/blink/renderer/platform/fonts/font_performance.h"
#include "third_party/blink/renderer/platform/fonts/font_platform_data_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_unique_name_lookup.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/web_memory_allocator_dump.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/web_process_memory_dump.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/gfx/font_list.h"

#if BUILDFLAG(IS_WIN)
#include "third_party/skia/include/ports/SkTypeface_win.h"
#endif

namespace blink {

const char kColorEmojiLocale[] = "und-Zsye";

#if BUILDFLAG(IS_ANDROID)
extern const char kNotoColorEmojiCompat[] = "Noto Color Emoji Compat";
#endif

SkFontMgr* FontCache::static_font_manager_ = nullptr;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
float FontCache::device_scale_factor_ = 1.0;
#endif

#if BUILDFLAG(IS_WIN)
bool FontCache::antialiased_text_enabled_ = false;
bool FontCache::lcd_text_enabled_ = false;
bool FontCache::use_skia_font_fallback_ = false;
static bool should_use_test_font_mgr = false;
#endif  // BUILDFLAG(IS_WIN)

FontCache& FontCache::Get() {
  return FontGlobalContext::GetFontCache();
}

FontCache::FontCache()
    : font_manager_(sk_ref_sp(static_font_manager_)),
      font_platform_data_cache_(FontPlatformDataCache::Create()),
      font_data_cache_(FontDataCache::Create()) {
#if BUILDFLAG(IS_WIN)
  if (!font_manager_ || should_use_test_font_mgr) {
    // This code path is only for unit tests. This SkFontMgr does not work in
    // sandboxed environments, but injecting this initialization code to all
    // unit tests isn't easy.
    font_manager_ = SkFontMgr_New_DirectWrite();
    // Set |is_test_font_mgr_| to capture if this is not happening in the
    // production code. crbug.com/561873
    is_test_font_mgr_ = true;

    // Tests[1][2] construct |FontCache| without |static_font_manager|, but
    // these tests install font manager with dwrite proxy even if they don't
    // have remote end in browser.
    // [1] HtmlBasedUsernameDetectorTest.UserGroupAttributes
    // [2] RenderViewImplTest.OnDeleteSurroundingTextInCodePoints
    should_use_test_font_mgr = true;
  }
  DCHECK(font_manager_.get());
#endif
}

FontCache::~FontCache() = default;

#if !BUILDFLAG(IS_MAC)
FontPlatformData* FontCache::SystemFontPlatformData(
    const FontDescription& font_description) {
  const AtomicString& family = FontCache::SystemFontFamily();
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
  if (family.empty() || family == font_family_names::kSystemUi)
    return nullptr;
#else
  DCHECK(!family.empty() && family != font_family_names::kSystemUi);
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

#if !BUILDFLAG(IS_MAC)
  if (creation_params.CreationType() == kCreateFontByFamily &&
      creation_params.Family() == font_family_names::kSystemUi) {
    return SystemFontPlatformData(font_description);
  }
#endif

  return font_platform_data_cache_->GetOrCreateFontPlatformData(
      this, font_description, creation_params, alternate_font_name);
}

std::unique_ptr<FontPlatformData> FontCache::ScaleFontPlatformData(
    const FontPlatformData& font_platform_data,
    const FontDescription& font_description,
    const FontFaceCreationParams& creation_params,
    float font_size) {
  TRACE_EVENT0("fonts,ui", "FontCache::ScaleFontPlatformData");

#if BUILDFLAG(IS_MAC)
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
  Get().InvalidateShapeCache();
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

  return font_data_cache_->Get(platform_data, should_retain,
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
  base::ElapsedTimer timer;
  scoped_refptr<SimpleFontData> result = PlatformFallbackFontForCharacter(
      description, lookup_char, font_data_to_substitute, fallback_priority);
  FontPerformance::AddSystemFallbackFontTime(timer.Elapsed());
  return result;
}

void FontCache::ReleaseFontData(const SimpleFontData* font_data) {
  font_data_cache_->Release(font_data);
}

void FontCache::PurgePlatformFontDataCache() {
  TRACE_EVENT0("fonts,ui", "FontCache::PurgePlatformFontDataCache");
  font_platform_data_cache_->Purge(*font_data_cache_);
}

void FontCache::PurgeFallbackListShaperCache() {
  TRACE_EVENT0("fonts,ui", "FontCache::PurgeFallbackListShaperCache");
  fallback_list_shaper_cache_.clear();
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

  if (!font_data_cache_->Purge(purge_severity))
    return;

  PurgePlatformFontDataCache();
  PurgeFallbackListShaperCache();
}

void FontCache::AddClient(FontCacheClient* client) {
  CHECK(client);
  if (!font_cache_clients_) {
    font_cache_clients_ =
        MakeGarbageCollected<HeapHashSet<WeakMember<FontCacheClient>>>();
    LEAK_SANITIZER_IGNORE_OBJECT(&font_cache_clients_);
  }
  DCHECK(!font_cache_clients_->Contains(client));
  font_cache_clients_->insert(client);
}

uint16_t FontCache::Generation() {
  return generation_;
}

void FontCache::Invalidate() {
  TRACE_EVENT0("fonts,ui", "FontCache::Invalidate");
  font_platform_data_cache_->Clear();
  generation_++;

  if (font_cache_clients_) {
    for (const auto& client : *font_cache_clients_)
      client->FontCacheInvalidated();
  }

  Purge(kForcePurge);
}

void FontCache::CrashWithFontInfo(const FontDescription* font_description) {
  SkFontMgr* font_mgr = nullptr;
  int num_families = std::numeric_limits<int>::min();
  bool is_test_font_mgr = false;
  if (FontGlobalContext::TryGet()) {
    FontCache& font_cache = FontGlobalContext::GetFontCache();
#if BUILDFLAG(IS_WIN)
    is_test_font_mgr = font_cache.is_test_font_mgr_;
#endif
    font_mgr = font_cache.font_manager_.get();
    if (font_mgr)
      num_families = font_mgr->countFamilies();
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
  dump->AddScalar("size", "bytes", font_platform_data_cache_->ByteSize());
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
      FontGlobalContext::Get().GetFontUniqueNameLookup();
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

FontFallbackMap& FontCache::GetFontFallbackMap() {
  if (!font_fallback_map_) {
    font_fallback_map_ = MakeGarbageCollected<FontFallbackMap>(nullptr);
    AddClient(font_fallback_map_);
  }
  return *font_fallback_map_;
}

}  // namespace blink
