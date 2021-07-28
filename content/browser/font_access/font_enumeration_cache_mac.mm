// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/font_access/font_enumeration_cache_mac.h"

#import <AppKit/AppKit.h>
#import <CoreText/CoreText.h>

#include <cmath>
#include <limits>
#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/pass_key.h"
#include "content/browser/font_access/font_enumeration_cache.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {

base::ScopedCFTypeRef<CFStringRef> GetLocalizedString(CTFontDescriptorRef fd,
                                                      CFStringRef attribute) {
  return base::ScopedCFTypeRef<CFStringRef>(base::mac::CFCast<CFStringRef>(
      CTFontDescriptorCopyLocalizedAttribute(fd, attribute, nullptr)));
}

base::ScopedCFTypeRef<CFStringRef> GetString(CTFontDescriptorRef fd,
                                             CFStringRef attribute) {
  return base::ScopedCFTypeRef<CFStringRef>(base::mac::CFCast<CFStringRef>(
      CTFontDescriptorCopyAttribute(fd, attribute)));
}

// Utility function to pull out a floating point value from a dictionary and
// return it, returning a default value if the key is not present or the wrong
// type.
CGFloat GetCGFloatFromDictionary(CFDictionaryRef dict,
                                 CFStringRef key,
                                 CGFloat default_value) {
  CFNumberRef number =
      base::mac::GetValueFromDictionary<CFNumberRef>(dict, key);
  if (!number)
    return default_value;

  CGFloat value;
  if (CFNumberGetValue(number, kCFNumberCGFloatType, &value))
    return value;
  return default_value;
}

// Map CoreText value to a boolean for italic/oblique.
bool CTSlantToWebItalic(CGFloat slant) {
  return slant > 0;
}

// Map CoreText value to a font-weight (number in [1,1000]).
// https://drafts.csswg.org/css-fonts-4/#font-weight-prop
float CTWeightToWebWeight(CGFloat weight) {
  // `fnan` is used as a sentinel value.
  constexpr CGFloat fnan = std::numeric_limits<CGFloat>::quiet_NaN();
  constexpr struct {
    CGFloat limit;
    float weight;
  } map[] = {
      {-0.700f, 100},  // NSFontWeightUltraLight = -0.8
      {-0.500f, 200},  // NSFontWeightThin = -0.6
      {-0.200f, 300},  // NSFontWeightLight = -0.4
      {0.000f, 350},   //
      {0.115f, 400},   // NSFontWeightRegular = 0
      {0.265f, 500},   // NSFontWeightMedium = 0.23
      {0.350f, 600},   // NSFontWeightSemibold = 0.3
      {0.480f, 700},   // NSFontWeightBold = 0.4
      {0.590f, 800},   // NSFontWeightHeavy = 0.56
      {fnan, 900},     // NSFontWeightBlack = 0.62
  };

  for (auto entry : map) {
    if (weight < entry.limit || std::isnan(entry.limit))
      return entry.weight;
  }
  NOTREACHED();
  return 400.f;
}

// Map CoreText value to a font-stretch value (percentage).
// https://drafts.csswg.org/css-fonts-4/#propdef-font-stretch
float CTWidthToWebStretch(CGFloat width) {
  // `fnan` is used as a sentinel value.
  constexpr CGFloat fnan = std::numeric_limits<CGFloat>::quiet_NaN();
  constexpr struct {
    CGFloat limit;
    float stretch;
  } map[] = {
      {-0.875f, 0.500f},  // -1.00
      {-0.625f, 0.625f},  // -0.75
      {-0.375f, 0.750f},  // -0.50
      {-0.125f, 0.875f},  // -0.25
      {0.125f, 1.000f},   // 0.00
      {0.375f, 1.125f},   // 0.25
      {0.625f, 1.250f},   // 0.50
      {0.875f, 1.500f},   // 0.75
      {fnan, 2.000f},     // 1.00
  };

  for (auto entry : map) {
    if (width < entry.limit || std::isnan(entry.limit))
      return entry.stretch;
  }
  NOTREACHED();
  return 0.f;
}

}  // namespace

// static
base::SequenceBound<FontEnumerationCache>
FontEnumerationCache::CreateForTesting(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    absl::optional<std::string> locale_override) {
  return base::SequenceBound<FontEnumerationCacheMac>(
      std::move(task_runner), std::move(locale_override),
      base::PassKey<FontEnumerationCache>());
}

FontEnumerationCacheMac::FontEnumerationCacheMac(
    absl::optional<std::string> locale_override,
    base::PassKey<FontEnumerationCache>)
    : FontEnumerationCache(std::move(locale_override)) {}

FontEnumerationCacheMac::~FontEnumerationCacheMac() = default;

void FontEnumerationCacheMac::SchedulePrepareFontEnumerationCache() {
  DCHECK(base::FeatureList::IsEnabled(blink::features::kFontAccess));

  scoped_refptr<base::SequencedTaskRunner> results_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});

  results_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&FontEnumerationCacheMac::PrepareFontEnumerationCache,
                     // Safe because this is an initialized singleton.
                     base::Unretained(this)));
}

void FontEnumerationCacheMac::PrepareFontEnumerationCache() {
  DCHECK(!enumeration_cache_built_->IsSet());

  @autoreleasepool {
    // Metrics.
    const base::ElapsedTimer start_timer;
    auto font_enumeration_table =
        std::make_unique<blink::FontEnumerationTable>();

    CFTypeRef values[1] = {kCFBooleanTrue};
    base::ScopedCFTypeRef<CFDictionaryRef> options(CFDictionaryCreate(
        kCFAllocatorDefault,
        (const void**)kCTFontCollectionRemoveDuplicatesOption,
        (const void**)&values,
        /*numValues=*/1, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks));
    base::ScopedCFTypeRef<CTFontCollectionRef> collection(
        CTFontCollectionCreateFromAvailableFonts(options));

    base::ScopedCFTypeRef<CFArrayRef> font_descs(
        CTFontCollectionCreateMatchingFontDescriptors(collection));

    // Used to filter duplicates.
    std::set<std::string> fonts_seen;
    int duplicate_count = 0;

    for (CFIndex i = 0; i < CFArrayGetCount(font_descs); ++i) {
      CTFontDescriptorRef fd = base::mac::CFCast<CTFontDescriptorRef>(
          CFArrayGetValueAtIndex(font_descs, i));
      base::ScopedCFTypeRef<CFStringRef> cf_postscript_name =
          GetString(fd, kCTFontNameAttribute);
      base::ScopedCFTypeRef<CFStringRef> cf_full_name =
          GetLocalizedString(fd, kCTFontDisplayNameAttribute);
      base::ScopedCFTypeRef<CFStringRef> cf_family =
          GetString(fd, kCTFontFamilyNameAttribute);
      base::ScopedCFTypeRef<CFStringRef> cf_style =
          GetString(fd, kCTFontStyleNameAttribute);

      std::string postscript_name =
          base::SysCFStringRefToUTF8(cf_postscript_name.get());

      if (fonts_seen.count(postscript_name) != 0) {
        ++duplicate_count;
        // Skip duplicates.
        continue;
      }
      fonts_seen.insert(postscript_name);

      // These defaults should map to the default web values when passed to the
      // CTXXXToWebYYY functions.
      CGFloat slant = 0.0f;   // Maps to italic: false.
      CGFloat weight = 0.0f;  // Maps to weight: 400 (regular).
      CGFloat width = 0.0f;   // Maps to width: 100% (normal).

      base::ScopedCFTypeRef<CFDictionaryRef> traits_ref(
          base::mac::CFCast<CFDictionaryRef>(
              CTFontDescriptorCopyAttribute(fd, kCTFontTraitsAttribute)));
      if (traits_ref) {
        slant = GetCGFloatFromDictionary(traits_ref.get(), kCTFontSlantTrait,
                                         slant);
        weight = GetCGFloatFromDictionary(traits_ref.get(), kCTFontWeightTrait,
                                          weight);
        width = GetCGFloatFromDictionary(traits_ref.get(), kCTFontWidthTrait,
                                         width);
      }

      blink::FontEnumerationTable_FontMetadata* metadata =
          font_enumeration_table->add_fonts();
      metadata->set_postscript_name(std::move(postscript_name));
      metadata->set_full_name(base::SysCFStringRefToUTF8(cf_full_name.get()));
      metadata->set_family(base::SysCFStringRefToUTF8(cf_family.get()));
      metadata->set_style(base::SysCFStringRefToUTF8(cf_style.get()));
      metadata->set_italic(CTSlantToWebItalic(slant));
      metadata->set_weight(CTWeightToWebWeight(weight));
      metadata->set_stretch(CTWidthToWebStretch(width));
    }

    BuildEnumerationCache(std::move(font_enumeration_table));

    base::UmaHistogramCounts100(
        "Fonts.AccessAPI.EnumerationCache.DuplicateFontCount", duplicate_count);
    base::UmaHistogramMediumTimes("Fonts.AccessAPI.EnumerationTime",
                                  start_timer.Elapsed());
    // Respond to pending and future requests.
    StartCallbacksTaskQueue();
  }
}

}  // namespace content
