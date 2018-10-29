/*
 * Copyright (C) 2006, 2008 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2007-2008 Torch Mobile, Inc.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_CACHE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_CACHE_H_

#include <limits.h>

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/fonts/fallback_list_composite_key.h"
#include "third_party/blink/renderer/platform/fonts/font_cache_client.h"
#include "third_party/blink/renderer/platform/fonts/font_cache_key.h"
#include "third_party/blink/renderer/platform/fonts/font_data_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_face_creation_params.h"
#include "third_party/blink/renderer/platform/fonts/font_fallback_priority.h"
#include "third_party/blink/renderer/platform/fonts/font_platform_data.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_cache.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/cstring.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkString;
class SkTypeface;

namespace base {
namespace trace_event {
class ProcessMemoryDump;
}
}

namespace blink {

class FontFaceCreationParams;
class FontGlobalContext;
class FontDescription;
class SimpleFontData;

enum class AlternateFontName {
  kAllowAlternate,
  kNoAlternate,
  kLocalUniqueFace,
  kLastResort
};

typedef HashMap<unsigned,
                std::unique_ptr<FontPlatformData>,
                WTF::IntHash<unsigned>,
                WTF::UnsignedWithZeroKeyHashTraits<unsigned>>
    SizedFontPlatformDataSet;
typedef HashMap<FontCacheKey,
                SizedFontPlatformDataSet,
                FontCacheKeyHash,
                FontCacheKeyTraits>
    FontPlatformDataCache;
typedef HashMap<FallbackListCompositeKey,
                std::unique_ptr<ShapeCache>,
                FallbackListCompositeKeyHash,
                FallbackListCompositeKeyTraits>
    FallbackListShaperCache;

class PLATFORM_EXPORT FontCache {
  friend class FontCachePurgePreventer;

  WTF_MAKE_NONCOPYABLE(FontCache);
  USING_FAST_MALLOC(FontCache);

 public:
  static FontCache* GetFontCache();

  void ReleaseFontData(const SimpleFontData*);

  scoped_refptr<SimpleFontData> FallbackFontForCharacter(
      const FontDescription&,
      UChar32,
      const SimpleFontData* font_data_to_substitute,
      FontFallbackPriority = FontFallbackPriority::kText);

  // Also implemented by the platform.
  void PlatformInit();

  scoped_refptr<SimpleFontData> GetFontData(
      const FontDescription&,
      const AtomicString&,
      AlternateFontName = AlternateFontName::kAllowAlternate,
      ShouldRetain = kRetain);
  scoped_refptr<SimpleFontData> GetLastResortFallbackFont(const FontDescription&,
                                                   ShouldRetain = kRetain);
  SimpleFontData* GetNonRetainedLastResortFallbackFont(const FontDescription&);

  // Should be used in determining whether family names listed in font-family:
  // ... are available locally. Only returns true if family name matches.
  bool IsPlatformFamilyMatchAvailable(const FontDescription&,
                                      const AtomicString& family);

  // Should be used in determining whether the <abc> argument to local in
  // @font-face { ... src: local(<abc>) } are available locally, which should
  // match Postscript name or full font name. Compare
  // https://drafts.csswg.org/css-fonts-3/#src-desc
  // TODO crbug.com/627143 complete this and actually look at the right
  // namerecords.
  bool IsPlatformFontUniqueNameMatchAvailable(
      const FontDescription&,
      const AtomicString& unique_font_name);

  static String FirstAvailableOrFirst(const String&);

  // Returns the ShapeCache instance associated with the given cache key.
  // Creates a new instance as needed and as such is guaranteed not to return
  // a nullptr. Instances are managed by FontCache and are only guaranteed to
  // be valid for the duration of the current session, as controlled by
  // disable/enablePurging.
  ShapeCache* GetShapeCache(const FallbackListCompositeKey&);

  void AddClient(FontCacheClient*);

  unsigned short Generation();
  void Invalidate();

  sk_sp<SkFontMgr> FontManager() { return font_manager_; }
  static void SetFontManager(sk_sp<SkFontMgr>);

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  // These are needed for calling QueryRenderStyleForStrike, since
  // gfx::GetFontRenderParams makes distinctions based on DSF.
  static float DeviceScaleFactor() { return device_scale_factor_; }
  static void SetDeviceScaleFactor(float device_scale_factor) {
    device_scale_factor_ = device_scale_factor;
  }
#endif

#if !defined(OS_MACOSX)
  static const AtomicString& SystemFontFamily();
#else
  static const AtomicString& LegacySystemFontFamily();
#endif

#if !defined(OS_WIN) && !defined(OS_MACOSX)
  static void SetSystemFontFamily(const AtomicString&);
#endif

#if defined(OS_WIN)
  // TODO(https://crbug.com/808221) System font style configuration is not
  // related to FontCache. Move it somewhere else, e.g. to WebThemeEngine.
  static bool AntialiasedTextEnabled() { return antialiased_text_enabled_; }
  static bool LcdTextEnabled() { return lcd_text_enabled_; }
  static void SetAntialiasedTextEnabled(bool enabled) {
    antialiased_text_enabled_ = enabled;
  }
  static void SetLCDTextEnabled(bool enabled) { lcd_text_enabled_ = enabled; }
  static void AddSideloadedFontForTesting(sk_sp<SkTypeface>);
  // Functions to cache and retrieve the system font metrics.
  static void SetMenuFontMetrics(const wchar_t* family_name,
                                 int32_t font_height);
  static void SetSmallCaptionFontMetrics(const wchar_t* family_name,
                                         int32_t font_height);
  static void SetStatusFontMetrics(const wchar_t* family_name,
                                   int32_t font_height);
  static int32_t MenuFontHeight() { return menu_font_height_; }
  static const AtomicString& MenuFontFamily() {
    return *menu_font_family_name_;
  }
  static int32_t SmallCaptionFontHeight() { return small_caption_font_height_; }
  static const AtomicString& SmallCaptionFontFamily() {
    return *small_caption_font_family_name_;
  }
  static int32_t StatusFontHeight() { return status_font_height_; }
  static const AtomicString& StatusFontFamily() {
    return *status_font_family_name_;
  }
  static void SetUseSkiaFontFallback(bool use_skia_font_fallback) {
    use_skia_font_fallback_ = use_skia_font_fallback;
  }
#endif  // defined(OS_WIN)

  static void AcceptLanguagesChanged(const String&);

#if defined(OS_ANDROID)
  static AtomicString GetGenericFamilyNameForScript(
      const AtomicString& family_name,
      const FontDescription&);
#endif  // defined(OS_ANDROID)

#if defined(OS_LINUX)
  struct PlatformFallbackFont {
    String name;
    CString filename;
    int fontconfig_interface_id;
    int ttc_index;
    bool is_bold;
    bool is_italic;
  };
  static void GetFontForCharacter(UChar32,
                                  const char* preferred_locale,
                                  PlatformFallbackFont*);
#endif  // defined(OS_LINUX)

  scoped_refptr<SimpleFontData> FontDataFromFontPlatformData(
      const FontPlatformData*,
      ShouldRetain = kRetain,
      bool subpixel_ascent_descent = false);

  void InvalidateShapeCache();

  static void CrashWithFontInfo(const FontDescription*);

  // Memory reporting
  void DumpFontPlatformDataCache(base::trace_event::ProcessMemoryDump*);
  void DumpShapeResultCache(base::trace_event::ProcessMemoryDump*);

  ~FontCache() = default;

 private:
  scoped_refptr<SimpleFontData> PlatformFallbackFontForCharacter(
      const FontDescription&,
      UChar32,
      const SimpleFontData* font_data_to_substitute,
      FontFallbackPriority = FontFallbackPriority::kText);

  friend class FontGlobalContext;
  FontCache();

  void Purge(PurgeSeverity = kPurgeIfNeeded);

  void DisablePurging() { purge_prevent_count_++; }
  void EnablePurging() {
    DCHECK(purge_prevent_count_);
    if (!--purge_prevent_count_)
      Purge(kPurgeIfNeeded);
  }

  // FIXME: This method should eventually be removed.
  FontPlatformData* GetFontPlatformData(
      const FontDescription&,
      const FontFaceCreationParams&,
      AlternateFontName = AlternateFontName::kAllowAlternate);
#if !defined(OS_MACOSX)
  FontPlatformData* SystemFontPlatformData(const FontDescription&);
#endif  // !defined(OS_MACOSX)

  // These methods are implemented by each platform.
  std::unique_ptr<FontPlatformData> CreateFontPlatformData(
      const FontDescription&,
      const FontFaceCreationParams&,
      float font_size,
      AlternateFontName = AlternateFontName::kAllowAlternate);
  std::unique_ptr<FontPlatformData> ScaleFontPlatformData(
      const FontPlatformData&,
      const FontDescription&,
      const FontFaceCreationParams&,
      float font_size);

  sk_sp<SkTypeface> CreateTypeface(const FontDescription&,
                                   const FontFaceCreationParams&,
                                   CString& name);

#if defined(OS_ANDROID) || defined(OS_LINUX) || defined(OS_FUCHSIA)
  static AtomicString GetFamilyNameForCharacter(SkFontMgr*,
                                                UChar32,
                                                const FontDescription&,
                                                FontFallbackPriority);
#endif  // defined(OS_ANDROID) || defined(OS_LINUX) || defined(OS_FUCHSIA)

  scoped_refptr<SimpleFontData> FallbackOnStandardFontStyle(const FontDescription&,
                                                     UChar32);

  // Don't purge if this count is > 0;
  int purge_prevent_count_;

  sk_sp<SkFontMgr> font_manager_;

  // A leaky owning bare pointer.
  static SkFontMgr* static_font_manager_;

#if defined(OS_WIN)
  static bool antialiased_text_enabled_;
  static bool lcd_text_enabled_;
  static HashMap<String, sk_sp<SkTypeface>, CaseFoldingHash>* sideloaded_fonts_;
  // The system font metrics cache.
  static AtomicString* menu_font_family_name_;
  static int32_t menu_font_height_;
  static AtomicString* small_caption_font_family_name_;
  static int32_t small_caption_font_height_;
  static AtomicString* status_font_family_name_;
  static int32_t status_font_height_;
  static bool use_skia_font_fallback_;

  // Windows creates an SkFontMgr for unit testing automatically. This flag is
  // to ensure it's not happening in the production from the crash log.
  bool is_test_font_mgr_ = false;
#endif  // defined(OS_WIN)

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  static float device_scale_factor_;
#endif

  unsigned short generation_ = 0;
  bool platform_init_ = false;
  Persistent<HeapHashSet<WeakMember<FontCacheClient>>> font_cache_clients_;
  FontPlatformDataCache font_platform_data_cache_;
  FallbackListShaperCache fallback_list_shaper_cache_;
  FontDataCache font_data_cache_;

  void PurgePlatformFontDataCache();
  void PurgeFallbackListShaperCache();

  friend class SimpleFontData;  // For fontDataFromFontPlatformData
  friend class FontFallbackList;
};

class PLATFORM_EXPORT FontCachePurgePreventer {
  USING_FAST_MALLOC(FontCachePurgePreventer);
  WTF_MAKE_NONCOPYABLE(FontCachePurgePreventer);

 public:
  FontCachePurgePreventer() { FontCache::GetFontCache()->DisablePurging(); }
  ~FontCachePurgePreventer() { FontCache::GetFontCache()->EnablePurging(); }
};

AtomicString ToAtomicString(const SkString&);

}  // namespace blink

#endif
