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
#include "base/strings/escape.h"
#include "base/system/sys_info.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "skia/ext/font_utils.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/font_family_names.h"
#include "third_party/blink/renderer/platform/fonts/alternate_font_family.h"
#include "third_party/blink/renderer/platform/fonts/font_cache_client.h"
#include "third_party/blink/renderer/platform/fonts/font_data_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_fallback_map.h"
#include "third_party/blink/renderer/platform/fonts/font_fallback_priority.h"
#include "third_party/blink/renderer/platform/fonts/font_global_context.h"
#include "third_party/blink/renderer/platform/fonts/font_performance.h"
#include "third_party/blink/renderer/platform/fonts/font_platform_data_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_unique_name_lookup.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/web_memory_allocator_dump.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/web_process_memory_dump.h"
#include "third_party/blink/renderer/platform/json/json_parser.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/code_point_iterator.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/gfx/font_list.h"

#if BUILDFLAG(IS_WIN)
#include "third_party/skia/include/ports/SkTypeface_win.h"
#endif

namespace blink {

const char kColorEmojiLocale[] = "und-Zsye";
const char kMonoEmojiLocale[] = "und-Zsym";

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
static bool should_use_test_font_mgr = false;
#endif  // BUILDFLAG(IS_WIN)

FontCache& FontCache::Get() {
  return FontGlobalContext::GetFontCache();
}

FontCache::FontCache() : font_manager_(sk_ref_sp(static_font_manager_)) {
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

void FontCache::Trace(Visitor* visitor) const {
  visitor->Trace(font_cache_clients_);
  visitor->Trace(font_platform_data_cache_);
  visitor->Trace(fallback_list_shaper_cache_);
  visitor->Trace(font_data_cache_);
  visitor->Trace(font_fallback_map_);
}

#if !BUILDFLAG(IS_MAC)
const FontPlatformData* FontCache::SystemFontPlatformData(
    const FontDescription& font_description) {
  const AtomicString& family = FontCache::SystemFontFamily();
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA) || \
    BUILDFLAG(IS_IOS)
  if (family.empty() || family == font_family_names::kSystemUi)
    return nullptr;
#else
  DCHECK(!family.empty() && family != font_family_names::kSystemUi);
#endif
  return GetFontPlatformData(font_description, FontFaceCreationParams(family),
                             AlternateFontName::kNoAlternate);
}
#endif

const FontPlatformData* FontCache::GetFontPlatformData(
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

  return font_platform_data_cache_.GetOrCreateFontPlatformData(
      this, font_description, creation_params, alternate_font_name);
}

ShapeCache* FontCache::GetShapeCache(const FallbackListCompositeKey& key) {
  auto result = fallback_list_shaper_cache_.insert(key, nullptr);
  if (result.is_new_entry) {
    result.stored_value->value = MakeGarbageCollected<ShapeCache>();
  }
  return result.stored_value->value.Get();
}

void FontCache::SetFontManager(sk_sp<SkFontMgr> font_manager) {
  DCHECK(!static_font_manager_);
  static_font_manager_ = font_manager.release();
}

void FontCache::AcceptLanguagesChanged(const String& accept_languages) {
  LayoutLocale::AcceptLanguagesChanged(accept_languages);
  Get().InvalidateShapeCache();
}

const SimpleFontData* FontCache::GetFontData(
    const FontDescription& font_description,
    const AtomicString& family,
    AlternateFontName altername_font_name) {
  if (const FontPlatformData* platform_data = GetFontPlatformData(
          font_description,
          FontFaceCreationParams(
              AdjustFamilyNameToAvoidUnsupportedFonts(family)),
          altername_font_name)) {
    return FontDataFromFontPlatformData(
        platform_data, font_description.SubpixelAscentDescent());
  }

  return nullptr;
}

const SimpleFontData* FontCache::FontDataFromFontPlatformData(
    const FontPlatformData* platform_data,
    bool subpixel_ascent_descent) {
  return font_data_cache_.Get(platform_data, subpixel_ascent_descent);
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

const SimpleFontData* FontCache::FallbackFontForCharacter(
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
  const SimpleFontData* result = PlatformFallbackFontForCharacter(
      description, lookup_char, font_data_to_substitute, fallback_priority);
  FontPerformance::AddSystemFallbackFontTime(timer.Elapsed());
  return result;
}

void FontCache::PurgeFallbackListShaperCache() {
  TRACE_EVENT0("fonts,ui", "FontCache::PurgeFallbackListShaperCache");
  for (auto& shape_cache : fallback_list_shaper_cache_.Values()) {
    shape_cache->Clear();
  }
}

void FontCache::InvalidateShapeCache() {
  PurgeFallbackListShaperCache();
}

void FontCache::Purge() {
  // Ideally we should never be forcing the purge while the
  // FontCachePurgePreventer is in scope, but we call purge() at any timing
  // via MemoryPressureListenerRegistry.
  if (purge_prevent_count_)
    return;

  PurgeFallbackListShaperCache();
}

void FontCache::AddClient(FontCacheClient* client) {
  CHECK(client);
  DCHECK(!font_cache_clients_.Contains(client));
  font_cache_clients_.insert(client);
}

uint16_t FontCache::Generation() {
  return generation_;
}

void FontCache::Invalidate() {
  TRACE_EVENT0("fonts,ui", "FontCache::Invalidate");
  font_platform_data_cache_.Clear();
  font_data_cache_.Clear();
  generation_++;

  for (const auto& client : font_cache_clients_) {
    client->FontCacheInvalidated();
  }

  Purge();
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
  SkFontMgr* skia_default_font_mgr = skia::DefaultFontMgr().get();
  base::debug::Alias(&font_mgr);
  base::debug::Alias(&static_font_mgr);
  base::debug::Alias(&skia_default_font_mgr);

  FontDescription font_description_copy = *font_description;
  base::debug::Alias(&font_description_copy);
  base::debug::Alias(&is_test_font_mgr);
  base::debug::Alias(&num_families);

  CHECK(false);
}

void FontCache::DumpShapeResultCache(
    base::trace_event::ProcessMemoryDump* memory_dump) {
  DCHECK(IsMainThread());
  base::trace_event::MemoryAllocatorDump* dump =
      memory_dump->CreateAllocatorDump("font_caches/shape_caches");
  size_t shape_result_cache_size = 0;
  for (const auto& shape_cache : fallback_list_shaper_cache_.Values()) {
    shape_result_cache_size += shape_cache->ByteSize();
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

  if (IsEmojiPresentationEmoji(fallback_priority)) {
    result.push_back(kColorEmojiLocale);
  } else if (IsTextPresentationEmoji(fallback_priority)) {
    result.push_back(kMonoEmojiLocale);
  }
  return result;
}

// TODO(crbug/342967843): In WebTest, Fuchsia initializes fonts by calling
// `skia::InitializeSkFontMgrForTest();` expecting that other code doesn't
// initialize SkFontMgr beforehand. But `FontCache::MaybePreloadSystemFonts()`
// breaks this expectation. So we don't provide
// `FontCache::MaybePreloadSystemFonts()` feature for Fuchsia for now.
#if BUILDFLAG(IS_FUCHSIA)
// static
void FontCache::MaybePreloadSystemFonts() {}
#else
// static
void FontCache::MaybePreloadSystemFonts() {
  static bool initialized = false;
  if (initialized) {
    return;
  }

  initialized = true;
  CHECK(IsMainThread());

  if (!base::FeatureList::IsEnabled(features::kPreloadSystemFonts)) {
    return;
  }

  const int kPhysicalMemoryGB =
      base::SysInfo::AmountOfPhysicalMemoryMB() / 1024;

  if (kPhysicalMemoryGB < features::kPreloadSystemFontsRequiredMemoryGB.Get()) {
    return;
  }

  std::unique_ptr<JSONArray> targets =
      JSONArray::From(ParseJSON(String::FromUTF8(
          base::UnescapeURLComponent(features::kPreloadSystemFontsTargets.Get(),
                                     base::UnescapeRule::SPACES))));

  if (!targets) {
    return;
  }

  const LayoutLocale& locale = LayoutLocale::GetDefault();

  for (wtf_size_t i = 0; i < targets->size(); ++i) {
    JSONObject* target = JSONObject::Cast(targets->at(i));
    bool success = true;
    String family;
    success &= target->GetString("family", &family);
    int weight;
    success &= target->GetInteger("weight", &weight);
    double specified_size;
    success &= target->GetDouble("size", &specified_size);
    double computed_size;
    success &= target->GetDouble("csize", &computed_size);
    String text;
    success &= target->GetString("text", &text);
    if (success) {
      TRACE_EVENT("fonts", "PreloadSystemFonts", "family", family, "weight",
                  weight, "specified_size", specified_size, "computed_size",
                  computed_size, "text", text);
      FontDescription font_description;
      const AtomicString family_atomic_string(family);
      FontFamily font_family(family_atomic_string,
                             FontFamily::Type::kFamilyName);
      font_description.SetFamily(font_family);
      font_description.SetWeight(FontSelectionValue(weight));
      font_description.SetLocale(&locale);
      font_description.SetSpecifiedSize(
          base::saturated_cast<float>(specified_size));
      font_description.SetComputedSize(
          base::saturated_cast<float>(computed_size));
      font_description.SetGenericFamily(FontDescription::kSansSerifFamily);
      const SimpleFontData* simple_font_data =
          FontCache::Get().GetFontData(font_description, AtomicString(family));
      if (simple_font_data) {
        for (UChar32 c : text) {
          Glyph glyph = simple_font_data->GlyphForCharacter(c);
          std::ignore = simple_font_data->BoundsForGlyph(glyph);
        }
      }
    }
  }
}
#endif  // BUILDFLAG(IS_FUCHSIA)

FontFallbackMap& FontCache::GetFontFallbackMap() {
  if (!font_fallback_map_) {
    font_fallback_map_ = MakeGarbageCollected<FontFallbackMap>(nullptr);
    AddClient(font_fallback_map_);
  }
  return *font_fallback_map_;
}

}  // namespace blink
