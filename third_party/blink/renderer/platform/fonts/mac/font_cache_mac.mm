/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#import "third_party/blink/renderer/platform/fonts/font_cache.h"

#import <AppKit/AppKit.h>
#import <CoreFoundation/CoreFoundation.h>
#import <CoreText/CoreText.h>
#include <Foundation/Foundation.h>

#include <memory>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/timer/elapsed_timer.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/font_family_names.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_face_creation_params.h"
#include "third_party/blink/renderer/platform/fonts/font_fallback_priority.h"
#include "third_party/blink/renderer/platform/fonts/font_platform_data.h"
#include "third_party/blink/renderer/platform/fonts/mac/font_matcher_mac.h"
#include "third_party/blink/renderer/platform/fonts/mac/font_platform_data_mac.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/web_test_support.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

using base::apple::CFToNSOwnershipCast;
using base::apple::CFToNSPtrCast;
using base::apple::NSToCFOwnershipCast;
using base::apple::NSToCFPtrCast;
using base::apple::ScopedCFTypeRef;

namespace blink {

namespace {

const float kCTNormalWeightValue = 0.0;
CTFontSymbolicTraits TraitsMask = kCTFontTraitItalic | kCTFontTraitBold |
                                  kCTFontTraitCondensed | kCTFontTraitExpanded;

ScopedCFTypeRef<CTFontRef> CreateCopyWithTraitsAndWeightFromFont(
    CTFontRef font,
    CTFontSymbolicTraits traits,
    float weight,
    float size) {
  ScopedCFTypeRef<CFStringRef> family_name(CTFontCopyFamilyName(font));
  // Some broken fonts may lack a postscript name (nameID="6"), full font
  // name (nameId="4") or family name (nameID="1") in the 'name' font table, see
  // https://learn.microsoft.com/en-us/typography/opentype/spec/name.
  // For these fonts `family_name` will be null, compare
  // https://crbug.com/1521364
  if (!family_name) {
    return ScopedCFTypeRef<CTFontRef>(nullptr);
  }

  NSDictionary* traits_dict = @{
    CFToNSPtrCast(kCTFontSymbolicTrait) : @(traits),
    CFToNSPtrCast(kCTFontWeightTrait) : @(weight),
  };
  NSDictionary* attributes = @{
    CFToNSPtrCast(kCTFontFamilyNameAttribute) :
        CFToNSPtrCast(family_name.get()),
    CFToNSPtrCast(kCTFontTraitsAttribute) : traits_dict,
  };

  ScopedCFTypeRef<CTFontDescriptorRef> descriptor(
      CTFontDescriptorCreateWithAttributes(NSToCFPtrCast(attributes)));
  // When we try to find the substitute font of "Menlo Regular" with italic
  // traits attribute for the selected code point using
  // `CTFontCreateCopyWithAttributes`, it gives us the same "Menlo Regular" font
  // rather than "Menlo Italic". So we are using
  // `CTFontCreateWithFontDescriptor` to find the better style match within the
  // family.
  return ScopedCFTypeRef<CTFontRef>(
      CTFontCreateWithFontDescriptor(descriptor.get(), size, nullptr));
}

bool IsLastResortFont(CTFontRef font) {
  ScopedCFTypeRef<CFStringRef> font_name(CTFontCopyPostScriptName(font));
  return font_name && CFStringCompare(font_name.get(), CFSTR("LastResort"),
                                      0) == kCFCompareEqualTo;
}

ScopedCFTypeRef<CTFontRef> GetSubstituteFont(CTFontRef ct_font,
                                             UChar32 character,
                                             float size) {
  auto bytes = base::bit_cast<std::array<UInt8, 4>>(character);
  ScopedCFTypeRef<CFStringRef> string(CFStringCreateWithBytes(
      kCFAllocatorDefault, std::data(bytes), std::size(bytes),
      kCFStringEncodingUTF32LE, false));
  CFRange range = CFRangeMake(0, CFStringGetLength(string.get()));

  ScopedCFTypeRef<CTFontRef> substitute_font;
  if (!ct_font) {
    // For some web fonts for which we use FreeType backend (for instance some
    // color fonts), `ct_font` is null. For these fonts we still want to have a
    // substitute font for a character. We are using the default value of
    // standard font from user settings defined in
    // `chrome/app/resources/locale_settings_mac.grd` as the font to substitute
    // from in `CTFontCreateForString`.
    ScopedCFTypeRef<CTFontRef> font_to_substitute(
        CTFontCreateWithName(CFSTR("Times"), size, nullptr));
    substitute_font.reset(
        CTFontCreateForString(font_to_substitute.get(), string.get(), range));
  } else {
    substitute_font.reset(CTFontCreateForString(ct_font, string.get(), range));
  }

  if (!substitute_font || IsLastResortFont(substitute_font.get())) {
    return ScopedCFTypeRef<CTFontRef>(nullptr);
  }

  ScopedCFTypeRef<CFStringRef> substitute_font_name(
      CTFontCopyName(substitute_font.get(), kCTFontFamilyNameKey));
  // System API might return colored "Apple Color Emoji" font for some emoji
  // codepoints. But if emoji codepoint was requested and
  // fallback_priority is not emoji presentation, it means that we need a
  // monochromatic (text) presentation of emoji. For that we use hardcoded
  // monochromatic emoji font.
  if (RuntimeEnabledFeatures::SystemFallbackEmojiVSSupportEnabled() &&
      CFStringCompare(substitute_font_name.get(), CFSTR("Apple Color Emoji"),
                      kCFCompareCaseInsensitive) == kCFCompareEqualTo &&
      Character::IsEmoji(character)) {
    ScopedCFTypeRef<CTFontRef> mono_emoji_font(
        CTFontCreateWithName(CFSTR("Apple Symbols"), size, nullptr));
    if (mono_emoji_font) {
      substitute_font.reset(
          CTFontCreateForString(mono_emoji_font.get(), string.get(), range));
    }
  }
  return substitute_font;
}

// Some fonts may have appearance information in the upper 16 bits,
// for example for "Times Roman" traits = (1 << 28) and for "Helvetica"
// traits = (1 << 30).
// We only need to care about typeface information in the lower 16 bits and
// need to check only whether the traits we care about mismatch (i.e.
// font-stretch, font-style and font-weight corresponding traits). So that
// later we can try to find a font within the same family with the desired
// typeface.
bool TraitsMismatch(CTFontSymbolicTraits desired_traits,
                    CTFontSymbolicTraits found_traits) {
  return (desired_traits & TraitsMask) != (found_traits & TraitsMask);
}

const FontPlatformData* GetAlternateFontPlatformData(
    const FontDescription& font_description,
    UChar32 character,
    const FontPlatformData& platform_data) {
  CTFontRef ct_font = platform_data.CtFont();

  float size = font_description.ComputedPixelSize();

  ScopedCFTypeRef<CTFontRef> substitute_font(
      GetSubstituteFont(ct_font, character, size));
  if (!substitute_font) {
    return nullptr;
  }

  auto get_ct_font_weight = [](CTFontRef font) -> float {
    NSDictionary* font_traits = CFToNSOwnershipCast(CTFontCopyTraits(font));

    float weight = kCTNormalWeightValue;
    if (font_traits) {
      NSNumber* weight_num = base::apple::ObjCCast<NSNumber>(
          font_traits[CFToNSPtrCast(kCTFontWeightTrait)]);
      if (weight_num) {
        weight = weight_num.floatValue;
      }
    }
    return weight;
  };

  CTFontSymbolicTraits traits;
  float weight = ToCTFontWeight(font_description.Weight());
  if (ct_font) {
    traits = CTFontGetSymbolicTraits(ct_font);
    if (platform_data.synthetic_bold_) {
      traits |= kCTFontTraitBold;
    }
    if (platform_data.synthetic_italic_) {
      traits |= kCTFontTraitItalic;
    }
  } else {
    traits = font_description.Style() ? kCTFontTraitItalic : 0;
  }

  CTFontSymbolicTraits substitute_font_traits =
      CTFontGetSymbolicTraits(substitute_font.get());
  float substitute_font_weight = get_ct_font_weight(substitute_font.get());

  if (TraitsMismatch(traits, substitute_font_traits) ||
      (weight != substitute_font_weight) || !ct_font) {
    ScopedCFTypeRef<CTFontRef> best_variation =
        CreateCopyWithTraitsAndWeightFromFont(substitute_font.get(), traits,
                                              weight, size);

    if (best_variation) {
      CTFontSymbolicTraits best_variation_font_traits =
          CTFontGetSymbolicTraits(best_variation.get());
      float best_variation_font_weight =
          get_ct_font_weight(best_variation.get());
      ScopedCFTypeRef<CFCharacterSetRef> char_set(
          CTFontCopyCharacterSet(best_variation.get()));
      if ((!ct_font || best_variation_font_traits != substitute_font_traits ||
           best_variation_font_weight != substitute_font_weight) &&
          char_set &&
          CFCharacterSetIsLongCharacterMember(char_set.get(), character)) {
        substitute_font = best_variation;
        substitute_font_traits = CTFontGetSymbolicTraits(substitute_font.get());
      }
    }
  }

  bool synthetic_bold = (traits & kCTFontTraitBold) &&
                        !(substitute_font_traits & kCTFontTraitBold);
  bool synthetic_italic = (traits & kCTFontTraitItalic) &&
                          !(substitute_font_traits & kCTFontTraitItalic);

  return FontPlatformDataFromCTFont(
      substitute_font.get(), font_description.EffectiveFontSize(),
      font_description.SpecifiedSize(), synthetic_bold, synthetic_italic,
      font_description.TextRendering(), ResolvedFontFeatures(),
      platform_data.Orientation(), font_description.FontOpticalSizing(),
      nullptr);
}

bool IsSystemFontName(const AtomicString& font_name) {
  return !font_name.empty() && font_name[0] == '.';
}

void FontCacheRegisteredFontsChangedNotificationCallback(
    CFNotificationCenterRef,
    void* observer,
    CFStringRef name,
    const void*,
    CFDictionaryRef) {
  DCHECK_EQ(observer, &FontCache::Get());
  DCHECK(CFEqual(name, kCTFontManagerRegisteredFontsChangedNotification));
  FontCache::InvalidateFromAnyThread();
}

}  // namespace

const char kColorEmojiFontMac[] = "Apple Color Emoji";

// static
const AtomicString& FontCache::LegacySystemFontFamily() {
  return font_family_names::kBlinkMacSystemFont;
}

// static
void FontCache::InvalidateFromAnyThread() {
  if (!IsMainThread()) {
    Thread::MainThread()
        ->GetTaskRunner(MainThreadTaskRunnerRestricted())
        ->PostTask(FROM_HERE,
                   WTF::BindOnce(&FontCache::InvalidateFromAnyThread));
    return;
  }
  FontCache::Get().Invalidate();
}

void FontCache::PlatformInit() {
  CFNotificationCenterAddObserver(
      CFNotificationCenterGetLocalCenter(), this,
      FontCacheRegisteredFontsChangedNotificationCallback,
      kCTFontManagerRegisteredFontsChangedNotification, /*object=*/nullptr,
      CFNotificationSuspensionBehaviorDeliverImmediately);
}

const SimpleFontData* FontCache::PlatformFallbackFontForCharacter(
    const FontDescription& font_description,
    UChar32 character,
    const SimpleFontData* font_data_to_substitute,
    FontFallbackPriority fallback_priority) {
  if (IsEmojiPresentationEmoji(fallback_priority)) {
    if (const SimpleFontData* emoji_font =
            GetFontData(font_description, AtomicString(kColorEmojiFontMac))) {
      return emoji_font;
    }
  }

  const FontPlatformData& platform_data =
      font_data_to_substitute->PlatformData();

  const FontPlatformData* alternate_font =
      GetAlternateFontPlatformData(font_description, character, platform_data);
  if (!alternate_font) {
    return nullptr;
  }

  return FontDataFromFontPlatformData(alternate_font);
}

const SimpleFontData* FontCache::GetLastResortFallbackFont(
    const FontDescription& font_description) {
  // FIXME: Would be even better to somehow get the user's default font here.
  // For now we'll pick the default that the user would get without changing
  // any prefs.
  const SimpleFontData* simple_font_data =
      GetFontData(font_description, font_family_names::kTimes,
                  AlternateFontName::kAllowAlternate);
  if (simple_font_data)
    return simple_font_data;

  // The Times fallback will almost always work, but in the highly unusual case
  // where the user doesn't have it, we fall back on Lucida Grande because
  // that's guaranteed to be there, according to Nathan Taylor. This is good
  // enough to avoid a crash at least.
  return GetFontData(font_description, font_family_names::kLucidaGrande,
                     AlternateFontName::kAllowAlternate);
}

const FontPlatformData* FontCache::CreateFontPlatformData(
    const FontDescription& font_description,
    const FontFaceCreationParams& creation_params,
    float size,
    AlternateFontName alternate_name) {
  // CoreText restricts the access to the system dot prefixed fonts, so return
  // nullptr to use fallback font instead.
  if (IsSystemFontName(creation_params.Family())) {
    return nullptr;
  }

  ScopedCFTypeRef<CTFontRef> matched_font;
  if (alternate_name == AlternateFontName::kLocalUniqueFace &&
      RuntimeEnabledFeatures::FontSrcLocalMatchingEnabled()) {
    matched_font = MatchUniqueFont(creation_params.Family(), size);
  } else if (creation_params.Family() == font_family_names::kSystemUi) {
    matched_font =
        MatchSystemUIFont(font_description.Weight(), font_description.Style(),
                          font_description.Stretch(), size);
  } else {
    matched_font = MatchFontFamily(
        creation_params.Family(), font_description.Weight(),
        font_description.Style(), font_description.Stretch(), size);
  }
  if (!matched_font)
    return nullptr;

  CTFontSymbolicTraits matched_font_traits =
      CTFontGetSymbolicTraits(matched_font.get());

  bool desired_bold = font_description.Weight() > FontSelectionValue(500);
  bool matched_font_bold = matched_font_traits & kCTFontTraitBold;
  bool synthetic_bold_requested = (desired_bold && !matched_font_bold) ||
                                  font_description.IsSyntheticBold();
  bool synthetic_bold =
      synthetic_bold_requested && font_description.SyntheticBoldAllowed();

  bool desired_italic = font_description.Style();
  bool matched_font_italic = matched_font_traits & kCTFontTraitItalic;
  bool synthetic_italic_requested = (desired_italic && !matched_font_italic) ||
                                    font_description.IsSyntheticItalic();
  bool synthetic_italic =
      synthetic_italic_requested && font_description.SyntheticItalicAllowed();

  // FontPlatformData::typeface() is null in the case of Chromium out-of-process
  // font loading failing.  Out-of-process loading occurs for registered fonts
  // stored in non-system locations.  When loading fails, we do not want to use
  // the returned FontPlatformData since it will not have a valid SkTypeface.
  const FontPlatformData* platform_data = FontPlatformDataFromCTFont(
      matched_font.get(), size, font_description.SpecifiedSize(),
      synthetic_bold, synthetic_italic, font_description.TextRendering(),
      ResolvedFontFeatures(), font_description.Orientation(),
      font_description.FontOpticalSizing(),
      font_description.VariationSettings());
  if (!platform_data || !platform_data->Typeface()) {
    return nullptr;
  }
  return platform_data;
}

}  // namespace blink
