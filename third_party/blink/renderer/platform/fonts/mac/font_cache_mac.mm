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
#include <memory>
#include "base/location.h"
#include "base/mac/foundation_util.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/font_family_names.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_face_creation_params.h"
#include "third_party/blink/renderer/platform/fonts/font_platform_data.h"
#include "third_party/blink/renderer/platform/fonts/mac/font_matcher_mac.h"
#include "third_party/blink/renderer/platform/fonts/mac/font_platform_data_mac.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/web_test_support.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

// Forward declare Mac SPIs.
// Request for public API: rdar://13803570
@interface NSFont (WebKitSPI)
+ (NSFont*)findFontLike:(NSFont*)font
              forString:(NSString*)string
              withRange:(NSRange)range
             inLanguage:(id)useNil;
+ (NSFont*)findFontLike:(NSFont*)font
           forCharacter:(UniChar)uc
             inLanguage:(id)useNil;
@end

namespace blink {

const char kColorEmojiFontMac[] = "Apple Color Emoji";

// static
const AtomicString& FontCache::LegacySystemFontFamily() {
  return font_family_names::kBlinkMacSystemFont;
}

static void InvalidateFontCache() {
  if (!IsMainThread()) {
    Thread::MainThread()->GetTaskRunner()->PostTask(
        FROM_HERE, WTF::Bind(&InvalidateFontCache));
    return;
  }
  FontCache::GetFontCache()->Invalidate();
}

static void FontCacheRegisteredFontsChangedNotificationCallback(
    CFNotificationCenterRef,
    void* observer,
    CFStringRef name,
    const void*,
    CFDictionaryRef) {
  DCHECK_EQ(observer, FontCache::GetFontCache());
  DCHECK(CFEqual(name, kCTFontManagerRegisteredFontsChangedNotification));
  InvalidateFontCache();
}

static bool UseHinting() {
  // Enable hinting only when antialiasing is disabled in web tests.
  return (WebTestSupport::IsRunningWebTest() &&
          !WebTestSupport::IsFontAntialiasingEnabledForTest());
}

void FontCache::PlatformInit() {
  CFNotificationCenterAddObserver(
      CFNotificationCenterGetLocalCenter(), this,
      FontCacheRegisteredFontsChangedNotificationCallback,
      kCTFontManagerRegisteredFontsChangedNotification, 0,
      CFNotificationSuspensionBehaviorDeliverImmediately);
}

static inline bool IsAppKitFontWeightBold(NSInteger app_kit_font_weight) {
  return app_kit_font_weight >= 7;
}

scoped_refptr<SimpleFontData> FontCache::PlatformFallbackFontForCharacter(
    const FontDescription& font_description,
    UChar32 character,
    const SimpleFontData* font_data_to_substitute,
    FontFallbackPriority fallback_priority) {
  if (fallback_priority == FontFallbackPriority::kEmojiEmoji) {
    scoped_refptr<SimpleFontData> emoji_font =
        GetFontData(font_description, AtomicString(kColorEmojiFontMac));
    if (emoji_font)
      return emoji_font;
  }

  // FIXME: We should fix getFallbackFamily to take a UChar32
  // and remove this split-to-UChar16 code.
  UChar code_units[2];
  int code_units_length;
  if (character <= 0xFFFF) {
    code_units[0] = character;
    code_units_length = 1;
  } else {
    code_units[0] = U16_LEAD(character);
    code_units[1] = U16_TRAIL(character);
    code_units_length = 2;
  }

  const FontPlatformData& platform_data =
      font_data_to_substitute->PlatformData();
  NSFont* ns_font = base::mac::CFToNSCast(platform_data.CtFont());

  NSString* string =
      [[NSString alloc] initWithCharactersNoCopy:code_units
                                          length:code_units_length
                                    freeWhenDone:NO];
  NSFont* substitute_font =
      [NSFont findFontLike:ns_font
                 forString:string
                 withRange:NSMakeRange(0, code_units_length)
                inLanguage:nil];
  [string release];

  // FIXME: Remove this SPI usage: http://crbug.com/255122
  if (!substitute_font && code_units_length == 1)
    substitute_font =
        [NSFont findFontLike:ns_font forCharacter:code_units[0] inLanguage:nil];
  if (!substitute_font)
    return nullptr;

  // Use the family name from the AppKit-supplied substitute font, requesting
  // the traits, weight, and size we want. One way this does better than the
  // original AppKit request is that it takes synthetic bold and oblique into
  // account.  But it does create the possibility that we could end up with a
  // font that doesn't actually cover the characters we need.

  NSFontManager* font_manager = [NSFontManager sharedFontManager];

  NSFontTraitMask traits;
  NSInteger weight;
  CGFloat size;

  if (ns_font) {
    traits = [font_manager traitsOfFont:ns_font];
    if (platform_data.synthetic_bold_)
      traits |= NSBoldFontMask;
    if (platform_data.synthetic_italic_)
      traits |= NSFontItalicTrait;
    weight = [font_manager weightOfFont:ns_font];
    size = [ns_font pointSize];
  } else {
    // For custom fonts nsFont is nil.
    traits = font_description.Style() ? NSFontItalicTrait : 0;
    weight = ToAppKitFontWeight(font_description.Weight());
    size = font_description.ComputedPixelSize();
  }

  NSFontTraitMask substitute_font_traits =
      [font_manager traitsOfFont:substitute_font];
  NSInteger substitute_font_weight =
      [font_manager weightOfFont:substitute_font];

  if (traits != substitute_font_traits || weight != substitute_font_weight ||
      !ns_font) {
    if (NSFont* best_variation =
            [font_manager fontWithFamily:[substitute_font familyName]
                                  traits:traits
                                  weight:weight
                                    size:size]) {
      if ((!ns_font ||
           [font_manager traitsOfFont:best_variation] !=
               substitute_font_traits ||
           [font_manager weightOfFont:best_variation] !=
               substitute_font_weight) &&
          [[best_variation coveredCharacterSet]
              longCharacterIsMember:character])
        substitute_font = best_variation;
    }
  }

  substitute_font = UseHinting() ? [substitute_font screenFont]
                                 : [substitute_font printerFont];

  substitute_font_traits = [font_manager traitsOfFont:substitute_font];
  substitute_font_weight = [font_manager weightOfFont:substitute_font];

  // TODO(eae): Remove once skia supports bold emoji. See
  // https://bugs.chromium.org/p/skia/issues/detail?id=4904
  // Bold emoji look the same as normal emoji, so syntheticBold isn't needed.
  bool synthetic_bold =
      IsAppKitFontWeightBold(weight) &&
      !IsAppKitFontWeightBold(substitute_font_weight) &&
      ![substitute_font.familyName isEqual:@"Apple Color Emoji"];

  std::unique_ptr<FontPlatformData> alternate_font = FontPlatformDataFromNSFont(
      substitute_font, platform_data.size(), synthetic_bold,
      (traits & NSFontItalicTrait) &&
          !(substitute_font_traits & NSFontItalicTrait),
      platform_data.Orientation(),
      nullptr);  // No variation paramaters in fallback.

  return FontDataFromFontPlatformData(alternate_font.get(), kDoNotRetain);
}

scoped_refptr<SimpleFontData> FontCache::GetLastResortFallbackFont(
    const FontDescription& font_description,
    ShouldRetain should_retain) {
  // FIXME: Would be even better to somehow get the user's default font here.
  // For now we'll pick the default that the user would get without changing
  // any prefs.
  scoped_refptr<SimpleFontData> simple_font_data =
      GetFontData(font_description, font_family_names::kTimes,
                  AlternateFontName::kAllowAlternate, should_retain);
  if (simple_font_data)
    return simple_font_data;

  // The Times fallback will almost always work, but in the highly unusual case
  // where the user doesn't have it, we fall back on Lucida Grande because
  // that's guaranteed to be there, according to Nathan Taylor. This is good
  // enough to avoid a crash at least.
  return GetFontData(font_description, font_family_names::kLucidaGrande,
                     AlternateFontName::kAllowAlternate, should_retain);
}

std::unique_ptr<FontPlatformData> FontCache::CreateFontPlatformData(
    const FontDescription& font_description,
    const FontFaceCreationParams& creation_params,
    float font_size,
    AlternateFontName alternate_name) {
  NSFontTraitMask traits = font_description.Style() ? NSFontItalicTrait : 0;
  float size = font_size;

  NSFont* matched_font = nullptr;
  if (alternate_name == AlternateFontName::kLocalUniqueFace &&
      RuntimeEnabledFeatures::FontSrcLocalMatchingEnabled()) {
    matched_font = MatchUniqueFont(creation_params.Family(), size);
  } else {
    matched_font = MatchNSFontFamily(creation_params.Family(), traits,
                                     font_description.Weight(), size);
  }
  if (!matched_font)
    return nullptr;

  NSFontManager* font_manager = [NSFontManager sharedFontManager];
  NSFontTraitMask actual_traits = 0;
  if (font_description.Style())
    actual_traits = [font_manager traitsOfFont:matched_font];
  NSInteger actual_weight = [font_manager weightOfFont:matched_font];

  NSFont* platform_font =
      UseHinting() ? [matched_font screenFont] : [matched_font printerFont];
  NSInteger app_kit_weight = ToAppKitFontWeight(font_description.Weight());

  // TODO(eae): Remove once skia supports bold emoji. See
  // https://bugs.chromium.org/p/skia/issues/detail?id=4904
  // Bold emoji look the same as normal emoji, so syntheticBold isn't needed.
  bool synthetic_bold = [platform_font.familyName isEqual:@"Apple Color Emoji"]
                            ? false
                            : (IsAppKitFontWeightBold(app_kit_weight) &&
                               !IsAppKitFontWeightBold(actual_weight)) ||
                                  font_description.IsSyntheticBold();

  bool synthetic_italic =
      ((traits & NSFontItalicTrait) && !(actual_traits & NSFontItalicTrait)) ||
      font_description.IsSyntheticItalic();

  // FontPlatformData::typeface() is null in the case of Chromium out-of-process
  // font loading failing.  Out-of-process loading occurs for registered fonts
  // stored in non-system locations.  When loading fails, we do not want to use
  // the returned FontPlatformData since it will not have a valid SkTypeface.
  std::unique_ptr<FontPlatformData> platform_data = FontPlatformDataFromNSFont(
      platform_font, size, synthetic_bold, synthetic_italic,
      font_description.Orientation(), font_description.VariationSettings());
  if (!platform_data->Typeface()) {
    return nullptr;
  }
  return platform_data;
}

}  // namespace blink
