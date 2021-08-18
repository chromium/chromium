// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/font_access/font_enumeration_cache_win.h"

#include <dwrite.h>
#include <wrl/client.h>

#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "content/browser/font_access/font_enumeration_cache.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "ui/gfx/win/direct_write.h"

namespace content {

namespace {

absl::optional<std::string> GetNativeString(
    Microsoft::WRL::ComPtr<IDWriteLocalizedStrings> names) {
  // Retrieve the native name. Try the "en-us" locale and if it's
  // not present, used the first available localized name.
  absl::optional<std::string> native =
      gfx::win::RetrieveLocalizedString(names.Get(), "en-us");
  if (!native) {
    native = gfx::win::RetrieveLocalizedString(names.Get(), "");
  }
  return native;
}

absl::optional<std::string> GetLocalizedString(
    Microsoft::WRL::ComPtr<IDWriteLocalizedStrings> names,
    const std::string& locale) {
  absl::optional<std::string> localized_name =
      gfx::win::RetrieveLocalizedString(names.Get(), locale);
  return localized_name;
}

// Map DWRITE_FONT_STYLE to a boolean for italic/oblique.
bool DWriteStyleToWebItalic(DWRITE_FONT_STYLE style) {
  return (style & (DWRITE_FONT_STYLE_ITALIC | DWRITE_FONT_STYLE_OBLIQUE)) != 0;
}

// Map DWRITE_FONT_WEIGHT to a font-weight (number in [1,1000]).
// https://drafts.csswg.org/css-fonts-4/#font-weight-prop
float DWriteWeightToWebWeight(DWRITE_FONT_WEIGHT weight) {
  // DirectWrite values already correspond to the web definition of
  // numbers in the range [1,1000] with 400 as normal.
  return weight;
}

// Map DWRITE_FONT_STRETCH to a font-stretch value (percentage).
// https://drafts.csswg.org/css-fonts-4/#propdef-font-stretch
float DWriteStretchToWebStretch(DWRITE_FONT_STRETCH stretch) {
  // DWRITE_FONT_STRETCH is an enumeration, so a more complex mapping or
  // interpolation is not necessary.
  switch (stretch) {
    case DWRITE_FONT_STRETCH_ULTRA_CONDENSED:
      return 0.5;
    case DWRITE_FONT_STRETCH_EXTRA_CONDENSED:
      return 0.625;
    case DWRITE_FONT_STRETCH_CONDENSED:
      return 0.75;
    case DWRITE_FONT_STRETCH_SEMI_CONDENSED:
      return 0.875;
    case DWRITE_FONT_STRETCH_UNDEFINED:
    case DWRITE_FONT_STRETCH_NORMAL:
      return 1.0f;
    case DWRITE_FONT_STRETCH_SEMI_EXPANDED:
      return 1.125f;
    case DWRITE_FONT_STRETCH_EXPANDED:
      return 1.25f;
    case DWRITE_FONT_STRETCH_EXTRA_EXPANDED:
      return 1.5f;
    case DWRITE_FONT_STRETCH_ULTRA_EXPANDED:
      return 2.0f;
  }
  NOTREACHED();
  return 1.0f;
}

std::unique_ptr<content::FontEnumerationCacheWin::FamilyDataResult>
ExtractNamesFromFamily(Microsoft::WRL::ComPtr<IDWriteFontCollection> collection,
                       uint32_t family_index,
                       const std::string& locale) {
  auto family_result =
      std::make_unique<content::FontEnumerationCacheWin::FamilyDataResult>();
  family_result->fonts =
      std::vector<blink::FontEnumerationTable_FontMetadata>();

  Microsoft::WRL::ComPtr<IDWriteFontFamily> family;
  HRESULT hr = collection->GetFontFamily(family_index, &family);
  if (FAILED(hr))
    return family_result;

  Microsoft::WRL::ComPtr<IDWriteLocalizedStrings> family_names;
  hr = family->GetFamilyNames(&family_names);
  if (FAILED(hr))
    return family_result;

  absl::optional<std::string> native_family_name =
      GetNativeString(family_names);
  if (!native_family_name)
    return family_result;

  uint32_t font_count = family->GetFontCount();
  for (uint32_t font_index = 0; font_index < font_count; ++font_index) {
    BOOL exists = false;

    Microsoft::WRL::ComPtr<IDWriteFont> font;
    {
      base::ScopedBlockingCall scoped_blocking_call(
          FROM_HERE, base::BlockingType::MAY_BLOCK);
      hr = family->GetFont(font_index, &font);
    }

    if (FAILED(hr))
      return family_result;

    // Skip this font if it's a simulation.
    if (font->GetSimulations() != DWRITE_FONT_SIMULATIONS_NONE)
      continue;

    // DWRITE_INFORMATIONAL_STRING_POSTSCRIPT_NAME and
    // DWRITE_INFORMATIONAL_STRING_FULL_NAME are only supported on Windows 7
    // with KB2670838 (https://support.microsoft.com/en-us/kb/2670838)
    // installed. It is possible to use a fallback as can be observed
    // in Firefox: https://bugzilla.mozilla.org/show_bug.cgi?id=947812 However,
    // this might not be worth the effort.

    // Extracting the postscript name.
    Microsoft::WRL::ComPtr<IDWriteLocalizedStrings> postscript_name;
    {
      base::ScopedBlockingCall scoped_blocking_call(
          FROM_HERE, base::BlockingType::MAY_BLOCK);
      hr = font->GetInformationalStrings(
          DWRITE_INFORMATIONAL_STRING_POSTSCRIPT_NAME, &postscript_name,
          &exists);
    }
    if (FAILED(hr))
      return family_result;

    if (!exists)
      return family_result;

    absl::optional<std::string> native_postscript_name =
        GetNativeString(postscript_name);
    if (!native_postscript_name) {
      return family_result;
    }

    // Extracting the full name.
    Microsoft::WRL::ComPtr<IDWriteLocalizedStrings> full_name;
    {
      base::ScopedBlockingCall scoped_blocking_call(
          FROM_HERE, base::BlockingType::MAY_BLOCK);
      hr = font->GetInformationalStrings(DWRITE_INFORMATIONAL_STRING_FULL_NAME,
                                         &full_name, &exists);
    }
    if (FAILED(hr))
      return family_result;

    if (!exists)
      return family_result;

    absl::optional<std::string> localized_full_name =
        GetLocalizedString(full_name, locale);
    if (!localized_full_name)
      localized_full_name = GetLocalizedString(full_name, "");
    if (!localized_full_name)
      localized_full_name = native_postscript_name;

    // Extracting style name.
    Microsoft::WRL::ComPtr<IDWriteLocalizedStrings> style_name;
    {
      base::ScopedBlockingCall scoped_blocking_call(
          FROM_HERE, base::BlockingType::MAY_BLOCK);
      hr = font->GetInformationalStrings(
          DWRITE_INFORMATIONAL_STRING_PREFERRED_SUBFAMILY_NAMES, &style_name,
          &exists);
    }
    if (FAILED(hr))
      return family_result;

    if (!exists) {
      {
        base::ScopedBlockingCall scoped_blocking_call(
            FROM_HERE, base::BlockingType::MAY_BLOCK);
        hr = font->GetInformationalStrings(
            DWRITE_INFORMATIONAL_STRING_WIN32_SUBFAMILY_NAMES, &style_name,
            &exists);
      }
      if (FAILED(hr))
        return family_result;
    }
    absl::optional<std::string> native_style_name;
    if (exists) {
      native_style_name = GetNativeString(style_name);
    }

    DWRITE_FONT_STRETCH stretch = font->GetStretch();
    DWRITE_FONT_STYLE style = font->GetStyle();
    DWRITE_FONT_WEIGHT weight = font->GetWeight();

    blink::FontEnumerationTable_FontMetadata metadata;
    metadata.set_postscript_name(native_postscript_name.value());
    metadata.set_full_name(localized_full_name.value());
    metadata.set_family(native_family_name.value());
    metadata.set_style(native_style_name ? native_style_name.value() : "");
    metadata.set_italic(DWriteStyleToWebItalic(style));
    metadata.set_weight(DWriteWeightToWebWeight(weight));
    metadata.set_stretch(DWriteStretchToWebStretch(stretch));

    family_result->fonts.push_back(std::move(metadata));
  }

  return family_result;
}
}  // namespace

// static
base::SequenceBound<FontEnumerationCache>
FontEnumerationCache::CreateForTesting(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    absl::optional<std::string> locale_override) {
  return base::SequenceBound<FontEnumerationCacheWin>(
      std::move(task_runner), std::move(locale_override),
      base::PassKey<FontEnumerationCache>());
}

FontEnumerationCacheWin::FontEnumerationCacheWin(
    absl::optional<std::string> locale_override,
    base::PassKey<FontEnumerationCache>)
    : FontEnumerationCache(std::move(locale_override)) {}

FontEnumerationCacheWin::~FontEnumerationCacheWin() = default;

FontEnumerationCacheWin::FamilyDataResult::~FamilyDataResult() = default;
FontEnumerationCacheWin::FamilyDataResult::FamilyDataResult() = default;

void FontEnumerationCacheWin::InitializeDirectWrite() {
  if (direct_write_initialized_)
    return;

  direct_write_initialized_ = true;

  Microsoft::WRL::ComPtr<IDWriteFactory> factory;
  gfx::win::CreateDWriteFactory(&factory);
  if (factory == nullptr) {
    // We won't be able to load fonts, but we should still return messages so
    // renderers don't hang.
    status_ = blink::mojom::FontEnumerationStatus::kUnexpectedError;
    return;
  }

  HRESULT hr = factory->GetSystemFontCollection(&collection_);
  DCHECK(SUCCEEDED(hr));

  if (!collection_) {
    status_ = blink::mojom::FontEnumerationStatus::kUnexpectedError;
    return;
  }
}

void FontEnumerationCacheWin::SchedulePrepareFontEnumerationCache() {
  DCHECK(base::FeatureList::IsEnabled(blink::features::kFontAccess));

  {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    InitializeDirectWrite();
  }

  scoped_refptr<base::SequencedTaskRunner> results_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});

  results_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&FontEnumerationCacheWin::PrepareFontEnumerationCache,
                     // Safe because this is an initialized singleton.
                     base::Unretained(this)));
}

void FontEnumerationCacheWin::PrepareFontEnumerationCache() {
  DCHECK(!enumeration_cache_built_->IsSet());

  font_enumeration_table_ = std::make_unique<blink::FontEnumerationTable>();

  {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);

    outstanding_family_results_ = collection_->GetFontFamilyCount();
  }

  std::string locale =
      locale_override_.value_or(base::i18n::GetConfiguredLocale());

  for (uint32_t family_index = 0; family_index < outstanding_family_results_;
       ++family_index) {
    // Specify base::ThreadPolicy::MUST_USE_FOREGROUND because a priority
    // inversion was observed https://crbug.com/960263 for the enumeration
    // of font names for @font-face src local matching.
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::ThreadPolicy::MUST_USE_FOREGROUND},
        base::BindOnce(&ExtractNamesFromFamily, collection_, family_index,
                       locale),
        base::BindOnce(
            &FontEnumerationCacheWin::AppendFontDataAndFinalizeIfNeeded,
            // Safe because this is an initialized singleton.
            base::Unretained(this)));
  }
}

void FontEnumerationCacheWin::AppendFontDataAndFinalizeIfNeeded(
    std::unique_ptr<FamilyDataResult> family_data_result) {
  outstanding_family_results_--;

  // If this task's response came late for some reason, we do not need the
  // results anymore and the table was already finalized.
  if (enumeration_cache_built_->IsSet())
    return;

  // Used to filter duplicates.
  std::set<std::string> fonts_seen;
  for (const auto& font_meta : family_data_result->fonts) {
    const std::string& postscript_name = font_meta.postscript_name();

    auto it_and_success = fonts_seen.emplace(postscript_name);
    if (!it_and_success.second) {
      // Skip duplicates.
      continue;
    }

    blink::FontEnumerationTable_FontMetadata* added_font_meta =
        font_enumeration_table_->add_fonts();
    *added_font_meta = font_meta;
  }

  if (!outstanding_family_results_) {
    FinalizeEnumerationCache();
  }
}

void FontEnumerationCacheWin::FinalizeEnumerationCache() {
  DCHECK(!enumeration_cache_built_->IsSet());

  // Ensures that the FontEnumerationTable gets released when this function goes
  // out of scope.
  std::unique_ptr<blink::FontEnumerationTable> enumeration_table(
      std::move(font_enumeration_table_));
  BuildEnumerationCache(std::move(enumeration_table));

  // Respond to pending and future requests.
  StartCallbacksTaskQueue();
}

}  // namespace content
