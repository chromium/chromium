// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/font_access/font_enumeration_cache_mac.h"

#import <AppKit/AppKit.h>
#import <CoreText/CoreText.h>

#include "base/feature_list.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
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

}  // namespace

FontEnumerationCacheMac::FontEnumerationCacheMac() = default;
FontEnumerationCacheMac::~FontEnumerationCacheMac() = default;

// static
FontEnumerationCache* FontEnumerationCache::GetInstance() {
  static base::NoDestructor<FontEnumerationCacheMac> instance;
  return instance.get();
}

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
  DCHECK(!enumeration_cache_built_.IsSet());

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
          GetLocalizedString(fd, kCTFontFamilyNameAttribute);

      std::string postscript_name =
          base::SysCFStringRefToUTF8(cf_postscript_name.get());

      if (fonts_seen.count(postscript_name) != 0) {
        ++duplicate_count;
        // Skip duplicates.
        continue;
      }
      fonts_seen.insert(postscript_name);

      blink::FontEnumerationTable_FontMetadata metadata;
      metadata.set_postscript_name(postscript_name.c_str());
      metadata.set_full_name(
          base::SysCFStringRefToUTF8(cf_full_name.get()).c_str());
      metadata.set_family(base::SysCFStringRefToUTF8(cf_family.get()).c_str());

      blink::FontEnumerationTable_FontMetadata* added_font_meta =
          font_enumeration_table->add_fonts();
      *added_font_meta = metadata;
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
