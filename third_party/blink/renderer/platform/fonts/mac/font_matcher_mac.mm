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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#import "third_party/blink/renderer/platform/fonts/mac/font_matcher_mac.h"

#import <AppKit/AppKit.h>
#import <CoreText/CoreText.h>
#import <Foundation/Foundation.h>
#import <math.h>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_selection_types.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#import "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#import "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"
#import "third_party/blink/renderer/platform/wtf/text/string_impl.h"

using base::apple::CFCast;
using base::apple::CFToNSOwnershipCast;
using base::apple::CFToNSPtrCast;
using base::apple::NSToCFOwnershipCast;
using base::apple::NSToCFPtrCast;
using base::apple::ObjCCast;
using base::apple::ScopedCFTypeRef;

// Forward declare Mac SPIs. `CTFontCopyVariationAxesInternal()` is working
// faster than a public `CTFontCopyVariationAxes()` because it does not
// localize variation axis name string, see
// https://github.com/WebKit/WebKit/commit/1842365d413ed87868e7d33d4fad1691fa3a8129.
// We don't need localized variation axis name, so we can use
// `CTFontCopyVariationAxesInternal()` instead.
// Request for public API: FB13788219.
extern "C" CFArrayRef CTFontCopyVariationAxesInternal(CTFontRef);

namespace blink {

namespace {

const FourCharCode kWeightTag = 'wght';
const FourCharCode kWidthTag = 'wdth';

const int kCTNormalTraitsValue = 0;

CTFontSymbolicTraits kImportantTraitsMask =
    kCTFontTraitItalic | kCTFontTraitBold | kCTFontTraitCondensed |
    kCTFontTraitExpanded;

const NSFontTraitMask SYNTHESIZED_FONT_TRAITS =
    (NSBoldFontMask | NSItalicFontMask);

const NSFontTraitMask IMPORTANT_FONT_TRAITS =
    (NSCompressedFontMask | NSCondensedFontMask | NSExpandedFontMask |
     NSItalicFontMask | NSNarrowFontMask | NSPosterFontMask |
     NSSmallCapsFontMask);

BOOL AcceptableChoice(NSFontTraitMask desired_traits,
                      NSFontTraitMask candidate_traits) {
  desired_traits &= ~SYNTHESIZED_FONT_TRAITS;
  return (candidate_traits & desired_traits) == desired_traits;
}

BOOL BetterChoice(NSFontTraitMask desired_traits,
                  NSInteger desired_weight,
                  NSFontTraitMask chosen_traits,
                  NSInteger chosen_weight,
                  NSFontTraitMask candidate_traits,
                  NSInteger candidate_weight) {
  if (!AcceptableChoice(desired_traits, candidate_traits))
    return NO;

  // A list of the traits we care about.
  // The top item in the list is the worst trait to mismatch; if a font has this
  // and we didn't ask for it, we'd prefer any other font in the family.
  const NSFontTraitMask kMasks[] = {NSPosterFontMask,    NSSmallCapsFontMask,
                                    NSItalicFontMask,    NSCompressedFontMask,
                                    NSCondensedFontMask, NSExpandedFontMask,
                                    NSNarrowFontMask};

  for (NSFontTraitMask mask : kMasks) {
    BOOL desired = (desired_traits & mask) != 0;
    BOOL chosen_has_unwanted_trait = desired != ((chosen_traits & mask) != 0);
    BOOL candidate_has_unwanted_trait =
        desired != ((candidate_traits & mask) != 0);
    if (!candidate_has_unwanted_trait && chosen_has_unwanted_trait)
      return YES;
    if (!chosen_has_unwanted_trait && candidate_has_unwanted_trait)
      return NO;
  }

  NSInteger chosen_weight_delta_magnitude = abs(chosen_weight - desired_weight);
  NSInteger candidate_weight_delta_magnitude =
      abs(candidate_weight - desired_weight);

  // If both are the same distance from the desired weight, prefer the candidate
  // if it is further from medium.
  if (chosen_weight_delta_magnitude == candidate_weight_delta_magnitude)
    return abs(candidate_weight - 6) > abs(chosen_weight - 6);

  // Otherwise, prefer the one closer to the desired weight.
  return candidate_weight_delta_magnitude < chosen_weight_delta_magnitude;
}

CTFontSymbolicTraits ComputeDesiredTraits(FontSelectionValue desired_weight,
                                          FontSelectionValue desired_slant,
                                          FontSelectionValue desired_width) {
  CTFontSymbolicTraits traits = 0;
  if (desired_weight >= kBoldThreshold) {
    traits |= kCTFontTraitBold;
  }
  if (desired_slant != kNormalSlopeValue) {
    traits |= kCTFontTraitItalic;
  }
  if (desired_width > kNormalWidthValue) {
    traits |= kCTFontTraitExpanded;
  }
  if (desired_width < kNormalWidthValue) {
    traits |= kCTFontTraitCondensed;
  }
  return traits;
}

NSFontTraitMask ComputeDesiredTraitsNS(FontSelectionValue desired_weight,
                                       FontSelectionValue desired_slant,
                                       FontSelectionValue desired_width) {
  NSFontTraitMask traits = 0;
  if (desired_weight >= kBoldThreshold) {
    traits |= NSBoldFontMask;
  }
  if (desired_slant != kNormalSlopeValue) {
    traits |= NSItalicFontMask;
  }
  if (desired_width > kNormalWidthValue) {
    traits |= NSExpandedFontMask;
  }
  if (desired_width < kNormalWidthValue) {
    traits |= NSCondensedFontMask;
  }
  return traits;
}

bool BetterChoiceCT(CTFontSymbolicTraits desired_traits,
                    int desired_weight,
                    CTFontSymbolicTraits chosen_traits,
                    int chosen_weight,
                    CTFontSymbolicTraits candidate_traits,
                    int candidate_weight) {
  // A list of the traits we care about.
  // The top item in the list is the worst trait to mismatch; if a font has this
  // and we didn't ask for it, we'd prefer any other font in the family.
  const CTFontSymbolicTraits kMasks[] = {kCTFontTraitCondensed,
                                         kCTFontTraitExpanded,
                                         kCTFontTraitItalic, kCTFontTraitBold};

  for (CTFontSymbolicTraits mask : kMasks) {
    // CoreText reports that "HiraginoSans-W5" font with AppKit weight 6 (which
    // we map to CSS weight 500), has a bold trait. Since we consider bold
    // threshold to be CSS weight 600, we will not match this font even if
    // `desired_weight=500` was requested, but instead we will match
    // "HiraginoSans-W4" with AppKit font weight 5 (CSS font weight 400).
    // This check ignores the bold trait value if the `candidate_weight` is the
    // same as requested.
    if (mask == kCTFontBoldTrait && candidate_weight == desired_weight &&
        chosen_weight != desired_weight) {
      return true;
    }
    bool desired = (desired_traits & mask) != 0;
    bool chosen_has_unwanted_trait = desired != ((chosen_traits & mask) != 0);
    bool candidate_has_unwanted_trait =
        desired != ((candidate_traits & mask) != 0);
    if (!candidate_has_unwanted_trait && chosen_has_unwanted_trait) {
      return true;
    }
    if (!chosen_has_unwanted_trait && candidate_has_unwanted_trait) {
      return false;
    }
  }

  int chosen_weight_delta_magnitude = abs(chosen_weight - desired_weight);
  int candidate_weight_delta_magnitude = abs(candidate_weight - desired_weight);

  // If both are the same distance from the desired weight, prefer the candidate
  // if it is further from medium, i.e. 500.
  if (chosen_weight_delta_magnitude == candidate_weight_delta_magnitude) {
    return abs(candidate_weight - 500) > abs(chosen_weight - 500);
  }

  // Otherwise, prefer the one closer to the desired weight.
  return candidate_weight_delta_magnitude < chosen_weight_delta_magnitude;
}

// This function is similar to `BestStyleMatchForFamily` except
// it uses AppKit `availableMembersOfFontFamily` instead of CoreText API
// to retrieve information about the fonts from the desired family.
// `availableMembersOfFontFamily` returns the list of name,
// weight and style of all fonts in family, which we are comparing against
// `desired_traits` and `desired_weight` to find the best matched font's name.
// Unlike `BestStyleMatchForFamily` where we create returned font from the best
// matched font's descriptor, here we are creating the return font from matched
// font's postscript name.
ScopedCFTypeRef<CTFontRef> BestStyleMatchForFamilyNS(
    CFStringRef family_name,
    CTFontSymbolicTraits desired_traits,
    int desired_weight,
    float size) {
  DCHECK(!RuntimeEnabledFeatures::FontFamilyStyleMatchingCTMigrationEnabled());
  NSFontManager* font_manager = NSFontManager.sharedFontManager;
  NSArray<NSArray*>* fonts =
      [font_manager availableMembersOfFontFamily:CFToNSPtrCast(family_name)];

  NSString* matched_font_name;
  CTFontSymbolicTraits chosen_traits;
  int chosen_weight;
  for (NSArray* font_info in fonts) {
    int candidate_weight = kNormalWeightValue;
    NSNumber* candidate_weight_ns = font_info[2];
    if (candidate_weight_ns) {
      candidate_weight = AppKitToCSSFontWeight(candidate_weight_ns.intValue);
    }

    CTFontSymbolicTraits candidate_traits = kCTNormalTraitsValue;
    NSNumber* candidate_traits_ns = font_info[3];
    if (candidate_traits_ns) {
      candidate_traits = candidate_traits_ns.intValue & kImportantTraitsMask;
    }

    if (!matched_font_name ||
        BetterChoiceCT(desired_traits, desired_weight, chosen_traits,
                       chosen_weight, candidate_traits, candidate_weight)) {
      matched_font_name = font_info[0];
      chosen_traits = candidate_traits;
      chosen_weight = candidate_weight;

      if (chosen_weight == desired_weight &&
          (chosen_traits & kImportantTraitsMask) ==
              (desired_traits & kImportantTraitsMask)) {
        break;
      }
    }
  }
  if (!matched_font_name) {
    return ScopedCFTypeRef<CTFontRef>(nullptr);
  }

  return ScopedCFTypeRef<CTFontRef>(
      CTFontCreateWithName(NSToCFPtrCast(matched_font_name), size, nullptr));
}

ScopedCFTypeRef<CTFontRef> BestStyleMatchForFamily(
    CFStringRef family_name,
    CTFontSymbolicTraits desired_traits,
    int desired_weight,
    float size) {
  DCHECK(RuntimeEnabledFeatures::FontFamilyStyleMatchingCTMigrationEnabled());
  // We need the order of the fonts in the family be same as in
  // `availableMembersOfFontFamily` so that the matching results are the same.
  // That's why we don't pass kCTFontCollectionRemoveDuplicatesOption, it might
  // change the order and therefore might change the matching result.
  ScopedCFTypeRef<CTFontCollectionRef> all_system_fonts(
      CTFontCollectionCreateFromAvailableFonts(nullptr));

  ScopedCFTypeRef<CFArrayRef> fonts_in_family(
      CTFontCollectionCreateMatchingFontDescriptorsForFamily(
          all_system_fonts.get(), family_name, NULL));
  if (!fonts_in_family) {
    return ScopedCFTypeRef<CTFontRef>(nullptr);
  }

  ScopedCFTypeRef<CTFontRef> matched_font_in_family;
  CTFontSymbolicTraits chosen_traits;
  int chosen_weight;

  for (CFIndex i = 0; i < CFArrayGetCount(fonts_in_family.get()); ++i) {
    CTFontDescriptorRef descriptor = CFCast<CTFontDescriptorRef>(
        CFArrayGetValueAtIndex(fonts_in_family.get(), i));
    if (!descriptor) {
      continue;
    }

    int candidate_traits = kCTNormalTraitsValue;
    int candidate_weight = kNormalWeightValue;
    ScopedCFTypeRef<CFTypeRef> traits_ref(
        CTFontDescriptorCopyAttribute(descriptor, kCTFontTraitsAttribute));
    NSDictionary* traits =
        CFToNSPtrCast(CFCast<CFDictionaryRef>(traits_ref.get()));
    if (traits) {
      NSNumber* candidate_traits_num =
          ObjCCast<NSNumber>(traits[CFToNSPtrCast(kCTFontSymbolicTrait)]);
      if (candidate_traits_num) {
        candidate_traits = candidate_traits_num.intValue;
      }

      NSNumber* candidate_weight_num =
          ObjCCast<NSNumber>(traits[CFToNSPtrCast(kCTFontWeightTrait)]);
      if (candidate_weight_num) {
        candidate_weight = ToCSSFontWeight(candidate_weight_num.floatValue);
      }
    }

    if (!matched_font_in_family ||
        BetterChoiceCT(desired_traits, desired_weight, chosen_traits,
                       chosen_weight, candidate_traits, candidate_weight)) {
      matched_font_in_family.reset(
          CTFontCreateWithFontDescriptor(descriptor, size, nullptr));
      chosen_traits = candidate_traits;
      chosen_weight = candidate_weight;
      // If we found a font with the exact weight and traits we asked for, we
      // can finish the search and return the font, otherwise we will continue
      // searching among the fonts in family to find the best (not necessarily
      // exact) match in traits and weight.
      if (chosen_weight == desired_weight &&
          (chosen_traits & kImportantTraitsMask) ==
              (desired_traits & kImportantTraitsMask)) {
        return matched_font_in_family;
      }
    }
  }
  return matched_font_in_family;
}

NSFont* MatchByPostscriptNameNS(const AtomicString& desired_family_string,
                                float size) {
  NSString* desired_family = desired_family_string;
  for (NSString* available_font in NSFontManager.sharedFontManager
           .availableFonts) {
    if ([desired_family caseInsensitiveCompare:available_font] ==
        NSOrderedSame) {
      return [NSFont fontWithName:available_font size:size];
    }
  }
  return nullptr;
}

}  // namespace

ScopedCFTypeRef<CTFontRef> MatchUniqueFont(const AtomicString& unique_font_name,
                                           float size) {
  // Note the header documentation: when matching, the system first searches for
  // fonts with its value as their PostScript name, then falls back to searching
  // for fonts with its value as their family name, and then falls back to
  // searching for fonts with its value as their display name.
  ScopedCFTypeRef<CFStringRef> desired_name(
      unique_font_name.Impl()->CreateCFString());
  ScopedCFTypeRef<CTFontRef> matched_font(
      CTFontCreateWithName(desired_name.get(), size, nullptr));
  DCHECK(matched_font);

  // CoreText will usually give us *something* but not always an exactly matched
  // font.
  ScopedCFTypeRef<CFStringRef> matched_postscript_name(
      CTFontCopyPostScriptName(matched_font.get()));
  ScopedCFTypeRef<CFStringRef> matched_full_font_name(
      CTFontCopyFullName(matched_font.get()));
  // If the found font does not match in PostScript name or full font name, it's
  // not the exact match that is required, so return nullptr.
  if (matched_postscript_name &&
      CFStringCompare(matched_postscript_name.get(), desired_name.get(),
                      kCFCompareCaseInsensitive) != kCFCompareEqualTo &&
      matched_full_font_name &&
      CFStringCompare(matched_full_font_name.get(), desired_name.get(),
                      kCFCompareCaseInsensitive) != kCFCompareEqualTo) {
    return ScopedCFTypeRef<CTFontRef>(nullptr);
  }

  return matched_font;
}

void ClampVariationValuesToFontAcceptableRange(
    ScopedCFTypeRef<CTFontRef> ct_font,
    FontSelectionValue& weight,
    FontSelectionValue& width) {
  // `CTFontCopyVariationAxesInternal()` is only supported on MacOS 12+, so
  // we are enabling it only on MacOS 13+ because these are our benchmarking
  // platforms.
  NSArray* all_axes;
  if (@available(macOS 13.0, *)) {
    all_axes =
        CFToNSOwnershipCast(CTFontCopyVariationAxesInternal(ct_font.get()));
  } else {
    all_axes = CFToNSOwnershipCast(CTFontCopyVariationAxes(ct_font.get()));
  }
  if (!all_axes) {
    return;
  }

  for (id id_axis in all_axes) {
    NSDictionary* axis = ObjCCast<NSDictionary>(id_axis);
    if (!axis) {
      continue;
    }

    NSNumber* axis_id = ObjCCast<NSNumber>(
        axis[CFToNSPtrCast(kCTFontVariationAxisIdentifierKey)]);
    if (!axis_id) {
      continue;
    }
    int axis_id_value = axis_id.intValue;

    NSNumber* axis_min_number = ObjCCast<NSNumber>(
        axis[CFToNSPtrCast(kCTFontVariationAxisMinimumValueKey)]);
    if (!axis_min_number) {
      continue;
    }
    double axis_min_value = axis_min_number.doubleValue;

    NSNumber* axis_max_number = ObjCCast<NSNumber>(
        axis[CFToNSPtrCast(kCTFontVariationAxisMaximumValueKey)]);
    if (!axis_max_number) {
      continue;
    }
    double axis_max_value = axis_max_number.doubleValue;

    FontSelectionRange capabilities_range({FontSelectionValue(axis_min_value),
                                           FontSelectionValue(axis_max_value)});

    if (axis_id_value == kWeightTag && weight != kNormalWeightValue) {
      weight = capabilities_range.clampToRange(weight);
    }
    if (axis_id_value == kWidthTag && width != kNormalWidthValue) {
      width = capabilities_range.clampToRange(width);
    }
  }
}

ScopedCFTypeRef<CTFontRef> MatchSystemUIFont(FontSelectionValue desired_weight,
                                             FontSelectionValue desired_slant,
                                             FontSelectionValue desired_width,
                                             float size) {
  ScopedCFTypeRef<CTFontRef> ct_font(
      CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, size, nullptr));
  // CoreText should always return a system-ui font.
  DCHECK(ct_font);

  CTFontSymbolicTraits desired_traits = 0;

  if (desired_slant != kNormalSlopeValue) {
    desired_traits |= kCTFontItalicTrait;
  }

  if (desired_weight >= kBoldThreshold) {
    desired_traits |= kCTFontBoldTrait;
  }

  if (desired_traits) {
    ct_font.reset(CTFontCreateCopyWithSymbolicTraits(
        ct_font.get(), size, nullptr, desired_traits, desired_traits));
  }

  if (desired_weight == kNormalWeightValue &&
      desired_width == kNormalWidthValue) {
    return ct_font;
  }

  ClampVariationValuesToFontAcceptableRange(ct_font, desired_weight,
                                            desired_width);

  NSMutableDictionary* variations = [NSMutableDictionary dictionary];
  if (desired_weight != kNormalWeightValue) {
    variations[@(kWeightTag)] = @(static_cast<float>(desired_weight));
  }
  if (desired_width != kNormalWidthValue) {
    variations[@(kWidthTag)] = @(static_cast<float>(desired_width));
  }

  NSDictionary* attributes = @{
    CFToNSPtrCast(kCTFontVariationAttribute) : variations,
  };

  ScopedCFTypeRef<CTFontDescriptorRef> var_font_desc(
      CTFontDescriptorCreateWithAttributes(NSToCFPtrCast(attributes)));

  return ScopedCFTypeRef<CTFontRef>(CTFontCreateCopyWithAttributes(
      ct_font.get(), size, nullptr, var_font_desc.get()));
}

// We first attempt to find a match by `desired_family_string` family name. If
// we failed to do so, we then try to find a match by postscript name. If during
// postscript matching we found font that has desired traits we will return it,
// otherwise we will do one more pass of family matching with the found with
// postscript matching font's family name.
// We perform matching by PostScript name for legacy and compatibility reasons
// (Safari also does it), although CSS specs do not require that, see
// crbug.com/641861.
ScopedCFTypeRef<CTFontRef> MatchFontFamily(
    const AtomicString& desired_family_string,
    FontSelectionValue desired_weight,
    FontSelectionValue desired_slant,
    FontSelectionValue desired_width,
    float size) {
  if (!desired_family_string) {
    return ScopedCFTypeRef<CTFontRef>(nullptr);
  }
  ScopedCFTypeRef<CFStringRef> desired_name(
      desired_family_string.Impl()->CreateCFString());

  // Due to the way we detect whether we can in-process load a font using
  // `CanLoadInProcess`, compare
  // third_party/blink/renderer/platform/fonts/mac/font_platform_data_mac.mm,
  // we cannot match the LastResort font on Mac.
  // TODO(crbug.com/1519877): We should allow matching LastResort font.
  if (CFStringCompare(desired_name.get(), CFSTR("LastResort"),
                      kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
    return ScopedCFTypeRef<CTFontRef>(nullptr);
  }

  CTFontSymbolicTraits desired_traits =
      ComputeDesiredTraits(desired_weight, desired_slant, desired_width);

  // CoreText's API for retrieving all system fonts from desired family
  // is working much slower than AppKits `availableMembersOfFontFamily`.
  // Filed in Apple Feedback Assistant, FB13615032.
  // This caused several performance regressions, compare
  // https://crbug.com/328483352. While we await feedback from
  // Apple, we re-introduce the previous AppKit-based style matching, using
  // NSFontManager availableMembersOfFontFamily API. We gate this change on a
  // separate flag.
  ScopedCFTypeRef<CTFontRef> match_in_family =
      RuntimeEnabledFeatures::FontFamilyStyleMatchingCTMigrationEnabled()
          ? BestStyleMatchForFamily(desired_name.get(), desired_traits,
                                    desired_weight, size)
          : BestStyleMatchForFamilyNS(desired_name.get(), desired_traits,
                                      desired_weight, size);

  if (!match_in_family) {
    // We first try to find font by postscript name. If the found font has
    // desired traits we will return it otherwise we will try to find the best
    // match in the found font's family.
    if (RuntimeEnabledFeatures::
            FontFamilyPostscriptMatchingCTMigrationEnabled()) {
      ScopedCFTypeRef<CTFontRef> matched_font(
          CTFontCreateWithName(desired_name.get(), size, nullptr));
      ScopedCFTypeRef<CFStringRef> matched_postscript_name(
          CTFontCopyPostScriptName(matched_font.get()));
      if (matched_postscript_name &&
          CFStringCompare(matched_postscript_name.get(), desired_name.get(),
                          kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
        CTFontSymbolicTraits traits =
            CTFontGetSymbolicTraits(matched_font.get());
        if ((desired_traits & traits) == desired_traits) {
          return matched_font;
        }

        ScopedCFTypeRef<CFStringRef> matched_family_name(
            CTFontCopyFamilyName(matched_font.get()));
        return RuntimeEnabledFeatures::
                       FontFamilyStyleMatchingCTMigrationEnabled()
                   ? BestStyleMatchForFamily(matched_family_name.get(),
                                             desired_traits, desired_weight,
                                             size)
                   : BestStyleMatchForFamilyNS(matched_family_name.get(),
                                               desired_traits, desired_weight,
                                               size);
      }
    } else {
      NSFont* postscript_match_font =
          MatchByPostscriptNameNS(desired_family_string, size);
      if (postscript_match_font) {
        NSFontTraitMask desired_traits_ns = ComputeDesiredTraitsNS(
            desired_weight, desired_slant, desired_width);
        NSFontManager* font_manager = NSFontManager.sharedFontManager;
        NSFontTraitMask traits =
            [font_manager traitsOfFont:postscript_match_font];
        if ((traits & desired_traits_ns) == desired_traits_ns) {
          return ScopedCFTypeRef<CTFontRef>(NSToCFOwnershipCast([font_manager
              convertFont:postscript_match_font
              toHaveTrait:desired_traits_ns]));
        }

        return RuntimeEnabledFeatures::
                       FontFamilyStyleMatchingCTMigrationEnabled()
                   ? BestStyleMatchForFamily(
                         NSToCFPtrCast(postscript_match_font.familyName),
                         desired_traits, desired_weight, size)
                   : BestStyleMatchForFamilyNS(
                         NSToCFPtrCast(postscript_match_font.familyName),
                         desired_traits, desired_weight, size);
      }
    }
  }
  return match_in_family;
}

// Family name is somewhat of a misnomer here.  We first attempt to find an
// exact match comparing the desiredFamily to the PostScript name of the
// installed fonts.  If that fails we then do a search based on the family
// names of the installed fonts.
NSFont* MatchNSFontFamily(const AtomicString& desired_family_string,
                          NSFontTraitMask desired_traits,
                          FontSelectionValue desired_weight,
                          float size) {
  DCHECK_NE(desired_family_string, FontCache::LegacySystemFontFamily());

  NSString* desired_family = desired_family_string;
  NSFontManager* font_manager = NSFontManager.sharedFontManager;

  // From macOS 10.15 `NSFontManager.availableFonts` does not list certain
  // fonts that availableMembersOfFontFamily actually shows results for, for
  // example "Hiragino Kaku Gothic ProN" is not listed, only Hiragino Sans is
  // listed. We previously enumerated availableFontFamilies and looked for a
  // case-insensitive string match here, but instead, we can rely on
  // availableMembersOfFontFamily here to do a case-insensitive comparison, then
  // set available_family to desired_family if the result was not empty.
  // See https://crbug.com/1000542
  NSString* available_family = nil;
  NSArray* fonts_in_family =
      [font_manager availableMembersOfFontFamily:desired_family];
  if (fonts_in_family && fonts_in_family.count) {
    available_family = desired_family;
  }

  NSInteger app_kit_font_weight = ToAppKitFontWeight(desired_weight);
  if (!available_family) {
    // Match by PostScript name.
    NSFont* name_matched_font =
        MatchByPostscriptNameNS(desired_family_string, size);
    if (!name_matched_font) {
      return nil;
    }

    available_family = name_matched_font.familyName;
    NSFontTraitMask desired_traits_for_name_match =
        desired_traits | (app_kit_font_weight >= 7 ? NSBoldFontMask : 0);

    // Special case Osaka-Mono.  According to <rdar://problem/3999467>, we
    // need to treat Osaka-Mono as fixed pitch.
    if ([available_family caseInsensitiveCompare:@"Osaka-Mono"] ==
            NSOrderedSame &&
        desired_traits_for_name_match == 0) {
      return name_matched_font;
    }

    NSFontTraitMask traits = [font_manager traitsOfFont:name_matched_font];
    if ((traits & desired_traits_for_name_match) ==
        desired_traits_for_name_match) {
      return [font_manager convertFont:name_matched_font
                           toHaveTrait:desired_traits_for_name_match];
    }
  }

  // Found a family, now figure out what weight and traits to use.
  BOOL chose_font = false;
  NSInteger chosen_weight = 0;
  NSFontTraitMask chosen_traits = 0;
  NSString* chosen_full_name = nil;

  NSArray<NSArray*>* fonts =
      [font_manager availableMembersOfFontFamily:available_family];
  for (NSArray* font_info in fonts) {
    // Each font is represented by an array of four elements:

    // TODO(https://crbug.com/1442008): The docs say that font_info[0] is the
    // PostScript name of the font, but it's treated as the full name here.
    // Either the docs are wrong and we should note that here for future readers
    // of the code, or the docs are right and we should fix this.
    // https://developer.apple.com/documentation/appkit/nsfontmanager/1462316-availablemembersoffontfamily
    NSString* font_full_name = font_info[0];
    // font_info[1] is "the part of the font name used in the font panel that's
    // not the font name". This is not needed.
    NSInteger font_weight = [font_info[2] intValue];
    NSFontTraitMask font_traits = [font_info[3] unsignedIntValue];

    BOOL new_winner;
    if (!chose_font) {
      new_winner = AcceptableChoice(desired_traits, font_traits);
    } else {
      new_winner =
          BetterChoice(desired_traits, app_kit_font_weight, chosen_traits,
                       chosen_weight, font_traits, font_weight);
    }

    if (new_winner) {
      chose_font = YES;
      chosen_weight = font_weight;
      chosen_traits = font_traits;
      chosen_full_name = font_full_name;

      if (chosen_weight == app_kit_font_weight &&
          (chosen_traits & IMPORTANT_FONT_TRAITS) ==
              (desired_traits & IMPORTANT_FONT_TRAITS))
        break;
    }
  }

  if (!chose_font)
    return nil;

  NSFont* font = [NSFont fontWithName:chosen_full_name size:size];

  if (!font)
    return nil;

  return font;
}

int ToAppKitFontWeight(FontSelectionValue font_weight) {
  float weight = font_weight;
  if (weight <= 50 || weight >= 950)
    return 5;

  size_t select_weight = roundf(weight / 100) - 1;
  static int app_kit_font_weights[] = {
      2,   // FontWeight100
      3,   // FontWeight200
      4,   // FontWeight300
      5,   // FontWeight400
      6,   // FontWeight500
      8,   // FontWeight600
      9,   // FontWeight700
      10,  // FontWeight800
      12,  // FontWeight900
  };
  DCHECK_GE(select_weight, 0ul);
  DCHECK_LE(select_weight, std::size(app_kit_font_weights));
  return app_kit_font_weights[select_weight];
}

// CoreText font weight ranges are taken from `GetFontWeightFromCTFont` in
// `ui/gfx/platform_font_mac.mm`
int ToCSSFontWeight(float ct_font_weight) {
  constexpr struct {
    float weight_lower;
    float weight_upper;
    int css_weight;
  } weights[] = {
      {-1.0, -0.70, 100},   // Thin (Hairline)
      {-0.70, -0.45, 200},  // Extra Light (Ultra Light)
      {-0.45, -0.10, 300},  // Light
      {-0.10, 0.10, 400},   // Normal (Regular)
      {0.10, 0.27, 500},    // Medium
      {0.27, 0.35, 600},    // Semi Bold (Demi Bold)
      {0.35, 0.50, 700},    // Bold
      {0.50, 0.60, 800},    // Extra Bold (Ultra Bold)
      {0.60, 1.0, 900},     // Black (Heavy)
  };
  for (const auto& item : weights) {
    if (item.weight_lower <= ct_font_weight &&
        ct_font_weight <= item.weight_upper) {
      return item.css_weight;
    }
  }
  return kNormalWeightValue;
}

float ToCTFontWeight(int css_weight) {
  if (css_weight <= 50 || css_weight >= 950) {
    return 0.0;
  }
  const float weights[] = {
      -0.80,  // Thin (Hairline)
      -0.60,  // Extra Light (Ultra Light)
      -0.40,  // Light
      0.0,    // Normal (Regular)
      0.23,   // Medium
      0.30,   // Semi Bold (Demi Bold)
      0.40,   // Bold
      0.56,   // Extra Bold (Ultra Bold)
      0.62,   // Black (Heavy)
  };
  int index = (css_weight - 50) / 100;
  return weights[index];
}

// AppKit font weight ranges are taken from `ToNSFontManagerWeight` in
// `ui/gfx/platform_font_mac.mm`.
int AppKitToCSSFontWeight(int appkit_font_weight) {
  if (appkit_font_weight < 0) {
    return kNormalWeightValue;
  }
  if (appkit_font_weight < 7) {
    return std::max((appkit_font_weight - 1) * 100,
                    static_cast<int>(kThinWeightValue));
  }
  return std::min((appkit_font_weight - 2) * 100,
                  static_cast<int>(kBlackWeightValue));
}

}  // namespace blink
