// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/font_access/font_enumeration_cache_fontconfig.h"

#include <fontconfig/fontconfig.h>

#include <memory>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/pass_key.h"
#include "content/browser/font_access/font_enumeration_cache.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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

// Utility function to pull out an integer value from a pattern and return it,
// returning a default value if the key is not present or the wrong type.
int GetIntegerFromFCPattern(FcPattern* pattern,
                            const char* object,
                            int index,
                            int default_value) {
  int result;
  if (FcPatternGetInteger(pattern, object, index, &result) == FcResultMatch)
    return result;
  return default_value;
}

// Map FC_SLANT_* values to a boolean for italic/oblique.
bool FCSlantToWebItalic(int slant) {
  return slant != FC_SLANT_ROMAN;
}

// Map FC_WEIGHT_* values to a font-weight (number in [1,1000]).
// https://drafts.csswg.org/css-fonts-4/#font-weight-prop
float FCWeightToWebWeight(int weight) {
  // A lookup table is used to provide values that exactly match CSS numeric
  // keywords. -1 is used as a sentinel value.
  constexpr struct {
    int limit;
    float weight;
  } map[] = {
      {20, 100.f},   // FC_WEIGHT_THIN = 0
      {45, 200.f},   // FC_WEIGHT_EXTRALIGHT = 40
      {52, 300.f},   // FC_WEIGHT_LIGHT = 50
      {65, 350.f},   // FC_WEIGHT_DEMILIGHT = 55
      {77, 375.f},   // FC_WEIGHT_BOOK = 75
      {90, 400.f},   // FC_WEIGHT_REGULAR = 80
      {140, 500.f},  // FC_WEIGHT_MEDIUM = 100
      {190, 600.f},  // FC_WEIGHT_DEMIBOLD = 180
      {202, 700.f},  // FC_WEIGHT_BOLD = 200
      {207, 800.f},  // FC_WEIGHT_EXTRABOLD = 205
      {212, 900.f},  // FC_WEIGHT_BLACK = 210
      {-1, 950.f}    // FC_WEIGHT_EXTRABLACK = 215
  };

  for (auto entry : map) {
    if (weight < entry.limit || entry.limit < 0)
      return entry.weight;
  }
  NOTREACHED();
  return 400;
}

// Map FC_WIDTH_* values to a font-stretch value (percentage).
// https://drafts.csswg.org/css-fonts-4/#propdef-font-stretch
float FCWidthToWebStretch(int width) {
  // FC_WIDTH_* values are effectively rounded percentages as integers. A lookup
  // table is used to provide values that exactly match CSS percentages. -1 is
  // used as a sentinel value.
  constexpr struct {
    int limit;
    float stretch;
  } map[] = {
      {56, 0.500f},   // FC_WIDTH_ULTRACONDENSED = 50
      {69, 0.625f},   // FC_WIDTH_EXTRACONDENSED = 63
      {81, 0.750f},   // FC_WIDTH_CONDENSED = 75
      {93, 0.875f},   // FC_WIDTH_SEMICONDENSED = 87
      {106, 1.000f},  // FC_WIDTH_NORMAL = 100
      {119, 1.125f},  // FC_WIDTH_SEMIEXPANDED = 113
      {137, 1.250f},  // FC_WIDTH_EXPANDED = 125
      {175, 1.500f},  // FC_WIDTH_EXTRAEXPANDED = 150
      {-1, 2.000f},   // FC_WIDTH_ULTRAEXPANDED = 200
  };

  for (auto entry : map) {
    if (width < entry.limit || entry.limit < 0)
      return entry.stretch;
  }
  NOTREACHED();
  return 100;
}

}  // namespace

// static
base::SequenceBound<FontEnumerationCache>
FontEnumerationCache::CreateForTesting(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    absl::optional<std::string> locale_override) {
  return base::SequenceBound<FontEnumerationCacheFontconfig>(
      std::move(task_runner), std::move(locale_override),
      base::PassKey<FontEnumerationCache>());
}

FontEnumerationCacheFontconfig::FontEnumerationCacheFontconfig(
    absl::optional<std::string> locale_override,
    base::PassKey<FontEnumerationCache>)
    : FontEnumerationCache(std::move(locale_override)) {}

FontEnumerationCacheFontconfig::~FontEnumerationCacheFontconfig() = default;

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
  DCHECK(!enumeration_cache_built_->IsSet());
  // Metrics.
  const base::ElapsedTimer start_timer;
  int incomplete_count = 0;
  int duplicate_count = 0;

  auto font_enumeration_table = std::make_unique<blink::FontEnumerationTable>();

  std::unique_ptr<FcObjectSet, decltype(&FcObjectSetDestroy)> object_set(
      FcObjectSetBuild(FC_POSTSCRIPT_NAME, FC_FULLNAME, FC_FAMILY, FC_STYLE,
                       nullptr),
      FcObjectSetDestroy);

  std::unique_ptr<FcFontSet, decltype(&FcFontSetDestroy)> fontset(
      ListFonts(object_set.get()), FcFontSetDestroy);

  base::UmaHistogramCustomCounts(
      "Fonts.AccessAPI.EnumerationCache.Fontconfig.FontCount", fontset->nfont,
      1, 5000, 50);

  // Used to filter duplicates.
  std::set<std::string> fonts_seen;

  for (int i = 0; i < fontset->nfont; ++i) {
    char* postscript_name;
    char* full_name;
    char* family;
    char* style;
    if (FcPatternGetString(fontset->fonts[i], FC_POSTSCRIPT_NAME, 0,
                           reinterpret_cast<FcChar8**>(&postscript_name)) !=
            FcResultMatch ||
        FcPatternGetString(fontset->fonts[i], FC_FULLNAME, 0,
                           reinterpret_cast<FcChar8**>(&full_name)) !=
            FcResultMatch ||
        FcPatternGetString(fontset->fonts[i], FC_FAMILY, 0,
                           reinterpret_cast<FcChar8**>(&family)) !=
            FcResultMatch ||
        FcPatternGetString(fontset->fonts[i], FC_STYLE, 0,
                           reinterpret_cast<FcChar8**>(&style)) !=
            FcResultMatch) {
      // Skip incomplete or malformed fonts.
      ++incomplete_count;
      continue;
    }

    if (fonts_seen.count(postscript_name) != 0) {
      ++duplicate_count;
      // Skip duplicates.
      continue;
    }

    fonts_seen.insert(postscript_name);

    // These properties may not be present, so defaults are provided and such
    // fonts are not skipped. These defaults should map to the default web
    // values when passed to the FCXXXToWebYYY functions.
    int slant =
        GetIntegerFromFCPattern(fontset->fonts[i], FC_SLANT, 0,
                                FC_SLANT_ROMAN);  // Maps to italic: false.
    int weight = GetIntegerFromFCPattern(
        fontset->fonts[i], FC_WEIGHT, 0,
        FC_WEIGHT_REGULAR);  // Maps to weight: 400 (regular).
    int width = GetIntegerFromFCPattern(
        fontset->fonts[i], FC_WIDTH, 0,
        FC_WIDTH_NORMAL);  // Maps to width: 100% (normal).

    blink::FontEnumerationTable_FontMetadata* metadata =
        font_enumeration_table->add_fonts();
    metadata->set_postscript_name(postscript_name);
    metadata->set_full_name(full_name);
    metadata->set_family(family);
    metadata->set_style(style);
    metadata->set_italic(FCSlantToWebItalic(slant));
    metadata->set_weight(FCWeightToWebWeight(weight));
    metadata->set_stretch(FCWidthToWebStretch(width));
  }

  base::UmaHistogramCounts100(
      "Fonts.AccessAPI.EnumerationCache.Fontconfig.IncompleteFontCount",
      incomplete_count);
  base::UmaHistogramCounts100(
      "Fonts.AccessAPI.EnumerationCache.DuplicateFontCount", duplicate_count);

  BuildEnumerationCache(std::move(font_enumeration_table));

  base::UmaHistogramMediumTimes("Fonts.AccessAPI.EnumerationTime",
                                start_timer.Elapsed());
  // Respond to pending and future requests.
  StartCallbacksTaskQueue();
}

}  // namespace content
