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

#import "third_party/blink/renderer/platform/fonts/mac/font_matcher_mac.h"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import <math.h>
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/stl_util.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#import "third_party/blink/renderer/platform/wtf/hash_set.h"
#import "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

@interface NSFont (YosemiteAdditions)
+ (NSFont*)systemFontOfSize:(CGFloat)size weight:(CGFloat)weight;
@end

namespace {

static CGFloat toYosemiteFontWeight(blink::FontSelectionValue font_weight) {
  static uint64_t ns_font_weights[] = {
      0xbfe99999a0000000,  // NSFontWeightUltraLight
      0xbfe3333340000000,  // NSFontWeightThin
      0xbfd99999a0000000,  // NSFontWeightLight
      0x0000000000000000,  // NSFontWeightRegular
      0x3fcd70a3e0000000,  // NSFontWeightMedium
      0x3fd3333340000000,  // NSFontWeightSemibold
      0x3fd99999a0000000,  // NSFontWeightBold
      0x3fe1eb8520000000,  // NSFontWeightHeavy
      0x3fe3d70a40000000,  // NSFontWeightBlack
  };
  if (font_weight <= 50 || font_weight >= 950)
    return ns_font_weights[3];

  size_t select_weight = roundf(font_weight / 100) - 1;
  DCHECK_GE(select_weight, 0ul);
  DCHECK_LE(select_weight, base::size(ns_font_weights));
  CGFloat* return_weight =
      reinterpret_cast<CGFloat*>(&ns_font_weights[select_weight]);
  return *return_weight;
}
}

namespace blink {

const NSFontTraitMask SYNTHESIZED_FONT_TRAITS =
    (NSBoldFontMask | NSItalicFontMask);

const NSFontTraitMask IMPORTANT_FONT_TRAITS =
    (NSCompressedFontMask | NSCondensedFontMask | NSExpandedFontMask |
     NSItalicFontMask | NSNarrowFontMask | NSPosterFontMask |
     NSSmallCapsFontMask);

static BOOL AcceptableChoice(NSFontTraitMask desired_traits,
                             NSFontTraitMask candidate_traits) {
  desired_traits &= ~SYNTHESIZED_FONT_TRAITS;
  return (candidate_traits & desired_traits) == desired_traits;
}

static BOOL BetterChoice(NSFontTraitMask desired_traits,
                         int desired_weight,
                         NSFontTraitMask chosen_traits,
                         int chosen_weight,
                         NSFontTraitMask candidate_traits,
                         int candidate_weight) {
  if (!AcceptableChoice(desired_traits, candidate_traits))
    return NO;

  // A list of the traits we care about.
  // The top item in the list is the worst trait to mismatch; if a font has this
  // and we didn't ask for it, we'd prefer any other font in the family.
  const NSFontTraitMask kMasks[] = {NSPosterFontMask,    NSSmallCapsFontMask,
                                    NSItalicFontMask,    NSCompressedFontMask,
                                    NSCondensedFontMask, NSExpandedFontMask,
                                    NSNarrowFontMask,    0};

  int i = 0;
  NSFontTraitMask mask;
  while ((mask = kMasks[i++])) {
    BOOL desired = (desired_traits & mask) != 0;
    BOOL chosen_has_unwanted_trait = desired != ((chosen_traits & mask) != 0);
    BOOL candidate_has_unwanted_trait =
        desired != ((candidate_traits & mask) != 0);
    if (!candidate_has_unwanted_trait && chosen_has_unwanted_trait)
      return YES;
    if (!chosen_has_unwanted_trait && candidate_has_unwanted_trait)
      return NO;
  }

  int chosen_weight_delta_magnitude = abs(chosen_weight - desired_weight);
  int candidate_weight_delta_magnitude = abs(candidate_weight - desired_weight);

  // If both are the same distance from the desired weight, prefer the candidate
  // if it is further from medium.
  if (chosen_weight_delta_magnitude == candidate_weight_delta_magnitude)
    return abs(candidate_weight - 6) > abs(chosen_weight - 6);

  // Otherwise, prefer the one closer to the desired weight.
  return candidate_weight_delta_magnitude < chosen_weight_delta_magnitude;
}

NSFont* MatchUniqueFont(const AtomicString& unique_font_name, float size) {
  // Testing with a large list of fonts available on Mac OS shows that matching
  // for kCTFontNameAttribute matches postscript name as well as full font name.
  NSString* desired_name = unique_font_name;
  NSDictionary* attributes = @{
    (NSString*)kCTFontNameAttribute : desired_name,
    (NSString*)kCTFontSizeAttribute : @(size)
  };
  base::ScopedCFTypeRef<CTFontDescriptorRef> descriptor(
      CTFontDescriptorCreateWithAttributes(base::mac::NSToCFCast(attributes)));

  base::ScopedCFTypeRef<CTFontRef> matched_font(
      CTFontCreateWithFontDescriptor(descriptor, 0, nullptr));

  // CoreText will usually give us *something* but not always an exactly
  // matched font.
  DCHECK(matched_font);
  base::ScopedCFTypeRef<CFStringRef> matched_font_ps_name(
      CTFontCopyName(matched_font, kCTFontPostScriptNameKey));
  base::ScopedCFTypeRef<CFStringRef> matched_font_full_font_name(
      CTFontCopyName(matched_font, kCTFontFullNameKey));
  // If the found font does not match in postscript name or full font name, it's
  // not the exact match that is required, so return nullptr.
  if (kCFCompareEqualTo != CFStringCompare(matched_font_ps_name,
                                           base::mac::NSToCFCast(desired_name),
                                           kCFCompareCaseInsensitive) &&
      kCFCompareEqualTo != CFStringCompare(matched_font_full_font_name,
                                           base::mac::NSToCFCast(desired_name),
                                           kCFCompareCaseInsensitive)) {
    return nullptr;
  }

  return [base::mac::CFToNSCast(matched_font.release()) autorelease];
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

  if (desired_family_string == font_family_names::kSystemUi) {
    NSFont* font = nil;
// Normally we'd use an availability macro here, but
// systemFontOfSize:weight: is available but not visible on macOS 10.10,
// so it's been forward declared earlier in this file.
// On OSX 10.10+, the default system font has more weights.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability"
    font = [NSFont systemFontOfSize:size
                             weight:toYosemiteFontWeight(desired_weight)];
#pragma clang diagnostic pop

    if (desired_traits & IMPORTANT_FONT_TRAITS)
      font = [[NSFontManager sharedFontManager] convertFont:font
                                                toHaveTrait:desired_traits];
    return font;
  }

  NSString* desired_family = desired_family_string;
  NSFontManager* font_manager = [NSFontManager sharedFontManager];

  // From Mac OS 10.15 [NSFontManager availableFonts] does not list certain
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
  if (fonts_in_family && [fonts_in_family count]) {
    available_family = desired_family;
  }

  int app_kit_font_weight = ToAppKitFontWeight(desired_weight);
  if (!available_family) {
    // Match by PostScript name.
    NSEnumerator* available_fonts =
        [[font_manager availableFonts] objectEnumerator];
    NSString* available_font;
    NSFont* name_matched_font = nil;
    NSFontTraitMask desired_traits_for_name_match =
        desired_traits | (app_kit_font_weight >= 7 ? NSBoldFontMask : 0);
    while ((available_font = [available_fonts nextObject])) {
      if ([desired_family caseInsensitiveCompare:available_font] ==
          NSOrderedSame) {
        name_matched_font = [NSFont fontWithName:available_font size:size];

        // Special case Osaka-Mono.  According to <rdar://problem/3999467>, we
        // need to treat Osaka-Mono as fixed pitch.
        if ([desired_family caseInsensitiveCompare:@"Osaka-Mono"] ==
                NSOrderedSame &&
            desired_traits_for_name_match == 0)
          return name_matched_font;

        NSFontTraitMask traits = [font_manager traitsOfFont:name_matched_font];
        if ((traits & desired_traits_for_name_match) ==
            desired_traits_for_name_match)
          return [font_manager convertFont:name_matched_font
                               toHaveTrait:desired_traits_for_name_match];

        available_family = [name_matched_font familyName];
        break;
      }
    }
  }

  // Found a family, now figure out what weight and traits to use.
  BOOL chose_font = false;
  int chosen_weight = 0;
  NSFontTraitMask chosen_traits = 0;
  NSString* chosen_full_name = 0;

  NSArray* fonts = [font_manager availableMembersOfFontFamily:available_family];
  unsigned n = [fonts count];
  unsigned i;
  for (i = 0; i < n; i++) {
    NSArray* font_info = [fonts objectAtIndex:i];

    // Array indices must be hard coded because of lame AppKit API.
    NSString* font_full_name = [font_info objectAtIndex:0];
    NSInteger font_weight = [[font_info objectAtIndex:2] intValue];

    NSFontTraitMask font_traits =
        [[font_info objectAtIndex:3] unsignedIntValue];

    BOOL new_winner;
    if (!chose_font)
      new_winner = AcceptableChoice(desired_traits, font_traits);
    else
      new_winner =
          BetterChoice(desired_traits, app_kit_font_weight, chosen_traits,
                       chosen_weight, font_traits, font_weight);

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

  NSFontTraitMask actual_traits = 0;
  if (desired_traits & NSFontItalicTrait)
    actual_traits = [font_manager traitsOfFont:font];
  int actual_weight = [font_manager weightOfFont:font];

  bool synthetic_bold = app_kit_font_weight >= 7 && actual_weight < 7;
  bool synthetic_italic = (desired_traits & NSFontItalicTrait) &&
                          !(actual_traits & NSFontItalicTrait);

  // There are some malformed fonts that will be correctly returned by
  // -fontWithFamily:traits:weight:size: as a match for a particular trait,
  // though -[NSFontManager traitsOfFont:] incorrectly claims the font does not
  // have the specified trait. This could result in applying
  // synthetic bold on top of an already-bold font, as reported in
  // <http://bugs.webkit.org/show_bug.cgi?id=6146>. To work around this
  // problem, if we got an apparent exact match, but the requested traits
  // aren't present in the matched font, we'll try to get a font from the same
  // family without those traits (to apply the synthetic traits to later).
  NSFontTraitMask non_synthetic_traits = desired_traits;

  if (synthetic_bold)
    non_synthetic_traits &= ~NSBoldFontMask;

  if (synthetic_italic)
    non_synthetic_traits &= ~NSItalicFontMask;

  if (non_synthetic_traits != desired_traits) {
    NSFont* font_without_synthetic_traits =
        [font_manager fontWithFamily:available_family
                              traits:non_synthetic_traits
                              weight:chosen_weight
                                size:size];
    if (font_without_synthetic_traits)
      font = font_without_synthetic_traits;
  }

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
  DCHECK_LE(select_weight, base::size(app_kit_font_weights));
  return app_kit_font_weights[select_weight];
}

}  // namespace blink
