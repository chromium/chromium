// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/font_access/font_enumeration_cache_fontconfig.h"

#include <fontconfig/fontconfig.h>

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {

std::unique_ptr<FcPattern, decltype(&FcPatternDestroy)> CreateFormatPattern(
    const char* format) {
  std::unique_ptr<FcPattern, decltype(&FcPatternDestroy)> pattern(
      FcPatternCreate(), FcPatternDestroy);
  FcPatternAddBool(pattern.get(), FC_SCALABLE, FcTrue);
  FcPatternAddString(pattern.get(), FC_FONTFORMAT,
                     reinterpret_cast<const FcChar8*>(format));
  return pattern;
}

// Returns a font set comprising of fonts in the provided object set.
FcFontSet* ListFonts(FcObjectSet* object_set) {
  FcFontSet* output = FcFontSetCreate();

  // See https://www.freetype.org/freetype2/docs/reference/ft2-font_formats.html
  // for the list of possible formats.
  const char* allowed_formats[] = {"TrueType", "CFF"};
  for (const auto* format : allowed_formats) {
    auto format_pattern = CreateFormatPattern(format);
    std::unique_ptr<FcFontSet, decltype(&FcFontSetDestroy)> fontset(
        FcFontList(nullptr, format_pattern.get(), object_set),
        FcFontSetDestroy);
    for (int j = 0; j < fontset->nfont; ++j) {
      FcPattern* font = fontset->fonts[j];
      // Increments the refcount for the font.
      FcPatternReference(font);
      FcBool result = FcFontSetAdd(output, font);
      DCHECK_EQ(result, FcTrue);
    }
  }
  return output;
}

}  // namespace

FontEnumerationCacheFontconfig::FontEnumerationCacheFontconfig() = default;
FontEnumerationCacheFontconfig::~FontEnumerationCacheFontconfig() = default;

// static
FontEnumerationCache* FontEnumerationCache::GetInstance() {
  static base::NoDestructor<FontEnumerationCacheFontconfig> instance;
  return instance.get();
}

void FontEnumerationCacheFontconfig::QueueShareMemoryRegionWhenReady(
    scoped_refptr<base::TaskRunner> task_runner,
    blink::mojom::FontAccessManager::EnumerateLocalFontsCallback callback) {
  DCHECK(base::FeatureList::IsEnabled(blink::features::kFontAccess));

  callbacks_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FontEnumerationCacheFontconfig::RunPendingCallback,
          // Safe because this is an initialized singleton.
          base::Unretained(this),
          CallbackOnTaskRunner(std::move(task_runner), std::move(callback))));

  if (!enumeration_cache_build_started_.IsSet()) {
    enumeration_cache_build_started_.Set();

    SchedulePrepareFontEnumerationCache();
  }
}

bool FontEnumerationCacheFontconfig::IsFontEnumerationCacheReady() {
  DCHECK(base::FeatureList::IsEnabled(blink::features::kFontAccess));

  return enumeration_cache_built_.IsSet() && IsFontEnumerationCacheValid();
}

void FontEnumerationCacheFontconfig::SchedulePrepareFontEnumerationCache() {
  DCHECK(base::FeatureList::IsEnabled(blink::features::kFontAccess));

  scoped_refptr<base::SequencedTaskRunner> results_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});

  results_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FontEnumerationCacheFontconfig::PrepareFontEnumerationCache,
          // Safe because this is an initialized singleton.
          base::Unretained(this)));
}

void FontEnumerationCacheFontconfig::PrepareFontEnumerationCache() {
  DCHECK(!enumeration_cache_built_.IsSet());
  // Metrics.
  const base::ElapsedTimer start_timer;
  int incomplete_count = 0;
  int dupe_count = 0;

  auto font_enumeration_table = std::make_unique<blink::FontEnumerationTable>();

  std::unique_ptr<FcObjectSet, decltype(&FcObjectSetDestroy)> object_set(
      FcObjectSetBuild(FC_POSTSCRIPT_NAME, FC_FULLNAME, FC_FAMILY, nullptr),
      FcObjectSetDestroy);

  std::unique_ptr<FcFontSet, decltype(&FcFontSetDestroy)> fontset(
      ListFonts(object_set.get()), FcFontSetDestroy);

  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Fonts.AccessAPI.EnumerationCache.Fontconfig.FontCount", fontset->nfont,
      1, 5000, 50);

  // Used to filter duplicates.
  std::set<std::string> fonts_seen;

  for (int i = 0; i < fontset->nfont; ++i) {
    char* postscript_name;
    char* full_name;
    char* family;
    if (FcPatternGetString(fontset->fonts[i], FC_POSTSCRIPT_NAME, 0,
                           reinterpret_cast<FcChar8**>(&postscript_name)) !=
            FcResultMatch ||
        FcPatternGetString(fontset->fonts[i], FC_FULLNAME, 0,
                           reinterpret_cast<FcChar8**>(&full_name)) !=
            FcResultMatch ||
        FcPatternGetString(fontset->fonts[i], FC_FAMILY, 0,
                           reinterpret_cast<FcChar8**>(&family)) !=
            FcResultMatch) {
      // Skip incomplete or malformed fonts.
      ++incomplete_count;
      continue;
    }

    if (fonts_seen.count(postscript_name) != 0) {
      ++dupe_count;
      // Skip duplicates.
      continue;
    }

    fonts_seen.insert(postscript_name);

    blink::FontEnumerationTable_FontMetadata metadata;
    metadata.set_postscript_name(postscript_name);
    metadata.set_full_name(full_name);
    metadata.set_family(family);

    blink::FontEnumerationTable_FontMetadata* added_font_meta =
        font_enumeration_table->add_fonts();
    *added_font_meta = metadata;
  }

  UMA_HISTOGRAM_COUNTS_100(
      "Fonts.AccessAPI.EnumerationCache.Fontconfig.IncompleteFontCount",
      incomplete_count);
  UMA_HISTOGRAM_COUNTS_100(
      "Fonts.AccessAPI.EnumerationCache.Fontconfig.DuplicateFontCount",
      dupe_count);

  enumeration_cache_memory_ = base::ReadOnlySharedMemoryRegion::Create(
      font_enumeration_table->ByteSizeLong());

  if (!IsFontEnumerationCacheValid() ||
      !font_enumeration_table->SerializeToArray(
          enumeration_cache_memory_.mapping.memory(),
          enumeration_cache_memory_.mapping.size())) {
    enumeration_cache_memory_ = base::MappedReadOnlyRegion();
  }

  enumeration_cache_built_.Set();

  UMA_HISTOGRAM_MEDIUM_TIMES("Fonts.AccessAPI.EnumerationTime",
                             start_timer.Elapsed());
  // Respond to pending and future requests.
  StartCallbacksTaskQueue();
}

}  // namespace content
