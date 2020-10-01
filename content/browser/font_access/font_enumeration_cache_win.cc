// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/font_access/font_enumeration_cache_win.h"

#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/timer/elapsed_timer.h"
#include "third_party/blink/public/common/features.h"
#include "ui/gfx/win/direct_write.h"

namespace content {

namespace {

// The following HRESULT code is also used in local font src matching.
constexpr HRESULT kErrorNoFullNameOrPostScriptName =
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xD103);
// Additional local custom interface specific HRESULT code to log font
// enumeration errors when reporting them in a UMA metric.
constexpr HRESULT kErrorNoFamilyName =
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xD104);

base::Optional<std::string> GetNativeString(
    Microsoft::WRL::ComPtr<IDWriteLocalizedStrings> names) {
  // Retrieve the native name. Try the "en-us" locale and if it's
  // not present, used the first available localized name.
  base::Optional<std::string> native =
      gfx::win::RetrieveLocalizedString(names.Get(), "en-us");
  if (!native) {
    native = gfx::win::RetrieveLocalizedString(names.Get(), "");
  }
  return native;
}

base::Optional<std::string> GetLocalizedString(
    Microsoft::WRL::ComPtr<IDWriteLocalizedStrings> names,
    const std::string& locale) {
  base::Optional<std::string> localized_name =
      gfx::win::RetrieveLocalizedString(names.Get(), locale);
  return localized_name;
}

std::unique_ptr<content::FontEnumerationCacheWin::FamilyDataResult>
ExtractNamesFromFamily(Microsoft::WRL::ComPtr<IDWriteFontCollection> collection,
                       uint32_t family_index) {
  auto family_result =
      std::make_unique<content::FontEnumerationCacheWin::FamilyDataResult>();
  family_result->fonts =
      std::vector<blink::FontEnumerationTable_FontMetadata>();
  family_result->exit_hresult = S_OK;

  std::string locale = base::i18n::GetConfiguredLocale();

  Microsoft::WRL::ComPtr<IDWriteFontFamily> family;
  Microsoft::WRL::ComPtr<IDWriteLocalizedStrings> family_names;

  HRESULT hr = collection->GetFontFamily(family_index, &family);
  if (FAILED(hr)) {
    family_result->exit_hresult = hr;
    return family_result;
  }

  hr = family->GetFamilyNames(&family_names);
  if (FAILED(hr)) {
    family_result->exit_hresult = hr;
    return family_result;
  }
  base::Optional<std::string> native_family_name =
      GetNativeString(family_names);
  if (!native_family_name) {
    family_result->exit_hresult = kErrorNoFamilyName;
    return family_result;
  }

  base::Optional<std::string> localized_family_name =
      GetLocalizedString(family_names.Get(), locale);
  if (!localized_family_name)
    localized_family_name = native_family_name;

  UINT32 font_count = family->GetFontCount();
  for (UINT32 font_index = 0; font_index < font_count; ++font_index) {
    BOOL exists = false;

    Microsoft::WRL::ComPtr<IDWriteFont> font;

    {
      base::ScopedBlockingCall scoped_blocking_call(
          FROM_HERE, base::BlockingType::MAY_BLOCK);
      hr = family->GetFont(font_index, &font);
    }

    if (FAILED(hr)) {
      family_result->exit_hresult = hr;
      return family_result;
    }

    // Skip this font if it's a simulation.
    if (font->GetSimulations() != DWRITE_FONT_SIMULATIONS_NONE)
      continue;

    Microsoft::WRL::ComPtr<IDWriteLocalizedStrings> postscript_name;
    Microsoft::WRL::ComPtr<IDWriteLocalizedStrings> full_name;

    // DWRITE_INFORMATIONAL_STRING_POSTSCRIPT_NAME and
    // DWRITE_INFORMATIONAL_STRING_FULL_NAME are only supported on Windows 7
    // with KB2670838 (https://support.microsoft.com/en-us/kb/2670838)
    // installed. It is possible to use a fallback as can be observed
    // in Firefox: https://bugzilla.mozilla.org/show_bug.cgi?id=947812 However,
    // this might not be worth the effort.

    {
      base::ScopedBlockingCall scoped_blocking_call(
          FROM_HERE, base::BlockingType::MAY_BLOCK);
      hr = font->GetInformationalStrings(
          DWRITE_INFORMATIONAL_STRING_POSTSCRIPT_NAME, &postscript_name,
          &exists);
    }
    if (FAILED(hr)) {
      family_result->exit_hresult = hr;
      return family_result;
    }
    if (!exists) {
      family_result->exit_hresult = kErrorNoFullNameOrPostScriptName;
      return family_result;
    }

    base::Optional<std::string> native_postscript_name =
        GetNativeString(postscript_name);
    if (!native_postscript_name) {
      family_result->exit_hresult = kErrorNoFullNameOrPostScriptName;
      return family_result;
    }

    {
      base::ScopedBlockingCall scoped_blocking_call(
          FROM_HERE, base::BlockingType::MAY_BLOCK);
      hr = font->GetInformationalStrings(DWRITE_INFORMATIONAL_STRING_FULL_NAME,
                                         &full_name, &exists);
    }
    if (FAILED(hr)) {
      family_result->exit_hresult = hr;
      return family_result;
    }
    if (!exists) {
      family_result->exit_hresult = kErrorNoFullNameOrPostScriptName;
      return family_result;
    }

    base::Optional<std::string> localized_full_name =
        GetLocalizedString(full_name, locale);
    if (!localized_full_name)
      localized_full_name = GetLocalizedString(full_name, "");
    if (!localized_full_name)
      localized_full_name = native_postscript_name;

    blink::FontEnumerationTable_FontMetadata metadata;
    metadata.set_postscript_name(native_postscript_name.value());
    metadata.set_full_name(localized_full_name.value());
    metadata.set_family(localized_family_name.value());

    family_result->fonts.push_back(std::move(metadata));
  }

  return family_result;
}
}  // namespace

FontEnumerationCacheWin::FontEnumerationCacheWin() = default;
FontEnumerationCacheWin::~FontEnumerationCacheWin() = default;

FontEnumerationCacheWin::FamilyDataResult::~FamilyDataResult() = default;
FontEnumerationCacheWin::FamilyDataResult::FamilyDataResult() = default;

// static
FontEnumerationCache* FontEnumerationCache::GetInstance() {
  static base::NoDestructor<FontEnumerationCacheWin> instance;
  return instance.get();
}

void FontEnumerationCacheWin::InitializeDirectWrite() {
  if (direct_write_initialized_)
    return;

  direct_write_initialized_ = true;

  Microsoft::WRL::ComPtr<IDWriteFactory> factory;
  gfx::win::CreateDWriteFactory(&factory);
  if (factory == nullptr) {
    // We won't be able to load fonts, but we should still return messages so
    // renderers don't hang.
    status_ = FontEnumerationStatus::kUnexpectedError;
    return;
  }

  HRESULT hr = factory->GetSystemFontCollection(&collection_);
  DCHECK(SUCCEEDED(hr));

  if (!collection_) {
    base::UmaHistogramSparse(
        "Fonts.AccessAPI.EnumerationCache.Dwrite.GetSystemFontCollectionResult",
        hr);
    status_ = FontEnumerationStatus::kUnexpectedError;
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

  enumeration_errors_.clear();

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
  DCHECK(!enumeration_cache_built_.IsSet());
  DCHECK(!enumeration_timer_);

  enumeration_timer_ = std::make_unique<base::ElapsedTimer>();

  font_enumeration_table_ = std::make_unique<blink::FontEnumerationTable>();

  {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);

    outstanding_family_results_ = collection_->GetFontFamilyCount();

    base::UmaHistogramCustomCounts(
        "Fonts.AccessAPI.EnumerationCache.Dwrite.FamilyCount",
        outstanding_family_results_, 1, 5000, 50);
  }

  for (UINT32 family_index = 0; family_index < outstanding_family_results_;
       ++family_index) {
    // Specify base::ThreadPolicy::MUST_USE_FOREGROUND because a priority
    // inversion was observed https://crbug.com/960263 for the enumeration
    // of font names for @font-face src local matching.
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::ThreadPolicy::MUST_USE_FOREGROUND},
        base::BindOnce(&ExtractNamesFromFamily, collection_, family_index),
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
  if (enumeration_cache_built_.IsSet())
    return;

  if (FAILED(family_data_result->exit_hresult))
    enumeration_errors_[family_data_result->exit_hresult]++;

  // Used to filter duplicates.
  std::set<std::string> fonts_seen;
  int duplicate_count = 0;

  for (const auto& font_meta : family_data_result->fonts) {
    const std::string& postscript_name = font_meta.postscript_name();

    if (fonts_seen.count(postscript_name) != 0) {
      ++duplicate_count;
      // Skip duplicates.
      continue;
    }
    fonts_seen.insert(postscript_name);

    blink::FontEnumerationTable_FontMetadata* added_font_meta =
        font_enumeration_table_->add_fonts();
    *added_font_meta = font_meta;
  }

  if (!outstanding_family_results_) {
    FinalizeEnumerationCache();
  }

  base::UmaHistogramCounts100(
      "Fonts.AccessAPI.EnumerationCache.DuplicateFontCount", duplicate_count);
}

void FontEnumerationCacheWin::FinalizeEnumerationCache() {
  DCHECK(!enumeration_cache_built_.IsSet());
  DCHECK(enumeration_timer_);

  if (enumeration_errors_.size() > 0) {
    auto most_frequent_hresult = std::max_element(
        std::begin(enumeration_errors_), std::end(enumeration_errors_),
        [](const decltype(enumeration_errors_)::value_type& a,
           decltype(enumeration_errors_)::value_type& b) {
          return a.second < b.second;
        });
    base::UmaHistogramSparse(
        "Fonts.AccessAPI.EnumerationCache.Dwrite."
        "MostFrequentEnumerationFailure",
        most_frequent_hresult->first);
  }

  // Ensures that the FontEnumerationTable gets released when this function goes
  // out of scope.
  std::unique_ptr<blink::FontEnumerationTable> enumeration_table(
      std::move(font_enumeration_table_));
  BuildEnumerationCache(std::move(enumeration_table));

  base::UmaHistogramMediumTimes("Fonts.AccessAPI.EnumerationTime",
                                enumeration_timer_->Elapsed());
  enumeration_timer_.reset();

  // Respond to pending and future requests.
  StartCallbacksTaskQueue();
}

}  // namespace content
