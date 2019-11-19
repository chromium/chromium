// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/dwrite_font_lookup_table_builder_win.h"

#include <dwrite.h>
#include <dwrite_2.h>
#include <set>
#include <utility>

#include "base/file_version_info.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/hash/hash.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "base/win/registry.h"
#include "content/browser/renderer_host/dwrite_font_file_util_win.h"
#include "content/browser/renderer_host/dwrite_font_proxy_impl_win.h"
#include "content/browser/renderer_host/dwrite_font_uma_logging_win.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/common/font_unique_name_lookup/font_table_matcher.h"
#include "third_party/blink/public/common/font_unique_name_lookup/font_table_persistence.h"
#include "third_party/blink/public/common/font_unique_name_lookup/icu_fold_case_util.h"
#include "ui/gfx/win/direct_write.h"

namespace content {

namespace {

const base::FilePath::CharType kProtobufFilename[] =
    FILE_PATH_LITERAL("font_unique_name_table.pb");

// Timeout after which font scanning and metadata extraction is stopped and the
// local lookup table is cleared. Font scanning and lookup table construction is
// only needed pre Windows 10. If the timeout is hit, no local font matching
// will be performed on this particular pre Win 10 system.
constexpr base::TimeDelta kFontIndexingTimeoutDefault =
    base::TimeDelta::FromMinutes(5);

// In timeout test case, slow down indexing of one font file to this percentage
// of the timeout value. Assuming that at least two fonts are indexed, the
// timeout should be usually hit during indexing the second font.
constexpr float kIndexingSlowDownForTestingPercentage = 0.75;

// Additional local custom interface specific HRESULT codes (also added to
// enums.xml) to mark font scanning implementation specific error situations, as
// part of reporting them in a UMA metric.
constexpr HRESULT kErrorFontScanningTimedOut =
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xD101);
constexpr HRESULT kErrorExtractingLocalizedStringsFailed =
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xD102);
constexpr HRESULT kErrorNoFullNameOrPostScriptName =
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xD103);

bool ExtractCaseFoldedLocalizedStrings(
    IDWriteLocalizedStrings* dwrite_localized_strings,
    std::vector<std::string>* localized_strings) {
  uint32_t strings_count = dwrite_localized_strings->GetCount();

  if (!strings_count)
    return false;

  localized_strings->reserve(localized_strings->size() + strings_count);
  for (UINT32 j = 0; j < strings_count; ++j) {
    UINT32 length;
    HRESULT hr = dwrite_localized_strings->GetStringLength(j, &length);
    if (FAILED(hr))
      continue;
    std::wstring localized_name;
    localized_name.resize(length + 1);
    hr = dwrite_localized_strings->GetString(j, &localized_name[0], length + 1);
    if (FAILED(hr)) {
      continue;
    }
    localized_name.resize(length);
    // The documentation for the API call does not specify an encoding but the
    // results are wchar_t and FireFox considers them UTF-16, as seen here:
    // https://dxr.mozilla.org/mozilla-central/source/gfx/thebes/gfxDWriteFontList.cpp#90
    // so we'll assume that.
    localized_strings->push_back(base::UTF16ToUTF8(
        base::i18n::FoldCase(base::string16(localized_name))));
  }
  return true;
}

bool EnsureCacheDirectory(base::FilePath cache_directory) {
  // If the directory does not exist already, ensure that the parent directory
  // exists, which is usually the User Data directory. If it exists, we can try
  // creating the cache directory.
  return !cache_directory.empty() &&
         (base::DirectoryExists(cache_directory) ||
          (base::DirectoryExists(cache_directory.DirName()) &&
           CreateDirectory(cache_directory)));
}

}  // namespace

DWriteFontLookupTableBuilder::FontFileWithUniqueNames::FontFileWithUniqueNames(
    blink::FontUniqueNameTable_UniqueFont&& font,
    std::vector<std::string>&& names)
    : font_entry(std::move(font)), extracted_names(std::move(names)) {}

DWriteFontLookupTableBuilder::FontFileWithUniqueNames::
    ~FontFileWithUniqueNames() = default;

DWriteFontLookupTableBuilder::FontFileWithUniqueNames::FontFileWithUniqueNames(
    DWriteFontLookupTableBuilder::FontFileWithUniqueNames&& other) = default;

DWriteFontLookupTableBuilder::FamilyResult::FamilyResult() = default;
DWriteFontLookupTableBuilder::FamilyResult::FamilyResult(FamilyResult&& other) =
    default;
DWriteFontLookupTableBuilder::FamilyResult::~FamilyResult() = default;

DWriteFontLookupTableBuilder::DWriteFontLookupTableBuilder()
    : font_indexing_timeout_(kFontIndexingTimeoutDefault) {
  InitializeCacheDirectoryFromProfile();
}

void DWriteFontLookupTableBuilder::InitializeCacheDirectoryFromProfile() {
  // In FontUniqueNameBrowserTest the DWriteFontLookupTableBuilder is
  // instantiated to configure the cache directory for testing explicitly before
  // GetContentClient() is available. Catch this case here. It is safe to not
  // set the cache directory here, as an invalid cache directory would be
  // detected by TableCacheFilePath and the LoadFromFile and PersistToFile
  // methods.
  cache_directory_ =
      GetContentClient() && GetContentClient()->browser()
          ? GetContentClient()->browser()->GetFontLookupTableCacheDir()
          : base::FilePath();
}

DWriteFontLookupTableBuilder::~DWriteFontLookupTableBuilder() = default;

base::ReadOnlySharedMemoryRegion
DWriteFontLookupTableBuilder::DuplicateMemoryRegion() {
  DCHECK(!TableCacheFilePath().empty())
      << "Ensure that a cache_directory_ is set (see "
         "InitializeCacheDirectoryFromProfile())";
  DCHECK(FontUniqueNameTableReady());
  return font_table_memory_.region.Duplicate();
}

bool DWriteFontLookupTableBuilder::IsFontUniqueNameTableValid() {
  return font_table_memory_.IsValid() && font_table_memory_.mapping.size();
}

void DWriteFontLookupTableBuilder::InitializeDirectWrite() {
  if (direct_write_initialized_)
    return;
  direct_write_initialized_ = true;

  Microsoft::WRL::ComPtr<IDWriteFactory> factory;
  gfx::win::CreateDWriteFactory(&factory);
  if (factory == nullptr) {
    // We won't be able to load fonts, but we should still return messages so
    // renderers don't hang if they for some reason send us a font message.
    return;
  }

  // QueryInterface for IDwriteFactory3, needed for MatchUniqueFont on Windows
  // 10. May fail on older versions, in which case, unique font matching must be
  // done through indexing system fonts using DWriteFontLookupTableBuilder.
  factory.As<IDWriteFactory3>(&factory3_);

  HRESULT hr = factory->GetSystemFontCollection(&collection_);
  DCHECK(SUCCEEDED(hr));

  if (!collection_) {
    base::UmaHistogramSparse(
        "DirectWrite.Fonts.Proxy.GetSystemFontCollectionResult", hr);
    LogMessageFilterError(MessageFilterError::ERROR_NO_COLLECTION);
    return;
  }
}

std::string DWriteFontLookupTableBuilder::ComputePersistenceHash() {
  // Build a hash from DWrite product version, browser major version and font
  // names and file paths as stored in the registry. The browser major version
  // is included to ensure that the cache is rebuild at least once for every
  // Chrome milestone release. DWrite DLL version is included to ensure that any
  // change in DWrite behavior after an update does not interfere with the
  // information we have in the cache. The font registry keys and values are
  // used to detect changes in installed fonts.

  std::unique_ptr<FileVersionInfo> dwrite_version_info =
      FileVersionInfo::CreateFileVersionInfo(
          base::FilePath(FILE_PATH_LITERAL("DWrite.dll")));

  DCHECK(dwrite_version_info);

  std::string dwrite_version =
      base::WideToUTF8(dwrite_version_info->product_version());

  std::string to_hash = dwrite_version;

  const wchar_t kFonts[] =
      L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Fonts";
  base::win::RegistryValueIterator it(HKEY_LOCAL_MACHINE, kFonts);
  DCHECK(it.ValueCount());
  for (; it.Valid(); ++it) {
    to_hash.append(base::WideToUTF8(it.Name()));
    to_hash.append(base::WideToUTF8(it.Value()));
  }

  DCHECK(GetContentClient());
  to_hash.append(
      GetContentClient()->browser()->GetUserAgentMetadata().major_version);

  uint32_t fonts_changed_hash = base::PersistentHash(to_hash);
  return std::to_string(fonts_changed_hash);
}

void DWriteFontLookupTableBuilder::SetCacheDirectoryForTesting(
    base::FilePath cache_directory) {
  cache_directory_ = cache_directory;
}

void DWriteFontLookupTableBuilder::SetCachingEnabledForTesting(
    bool caching_enabled) {
  caching_enabled_ = caching_enabled;
}

bool DWriteFontLookupTableBuilder::HasDWriteUniqueFontLookups() {
  InitializeDirectWrite();
  return factory3_;
}

void DWriteFontLookupTableBuilder::OverrideDWriteVersionChecksForTesting() {
  InitializeDirectWrite();
  factory3_.Reset();
}

base::TimeDelta DWriteFontLookupTableBuilder::IndexingTimeout() {
  return font_indexing_timeout_;
}

void DWriteFontLookupTableBuilder::PostCallbacks() {
  for (auto& pending_callback : pending_callbacks_) {
    pending_callback.task_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(pending_callback.mojo_callback),
                                  DuplicateMemoryRegion()));
  }
  pending_callbacks_.clear();
}

base::FilePath DWriteFontLookupTableBuilder::TableCacheFilePath() {
  if (!EnsureCacheDirectory(cache_directory_))
    return base::FilePath();
  return cache_directory_.Append(kProtobufFilename);
}

bool DWriteFontLookupTableBuilder::PersistToFile() {
  DCHECK(caching_enabled_);

  if (!IsFontUniqueNameTableValid())
    return false;

  return blink::font_table_persistence::PersistToFile(font_table_memory_,
                                                      TableCacheFilePath());
}

bool DWriteFontLookupTableBuilder::LoadFromFile() {
  DCHECK(caching_enabled_);
  DCHECK(!IsFontUniqueNameTableValid());

  return blink::font_table_persistence::LoadFromFile(TableCacheFilePath(),
                                                     &font_table_memory_);
}

DWriteFontLookupTableBuilder::CallbackOnTaskRunner::CallbackOnTaskRunner(
    scoped_refptr<base::SequencedTaskRunner> runner,
    blink::mojom::DWriteFontProxy::GetUniqueNameLookupTableCallback callback)
    : task_runner(std::move(runner)), mojo_callback(std::move(callback)) {}

DWriteFontLookupTableBuilder::CallbackOnTaskRunner::CallbackOnTaskRunner(
    CallbackOnTaskRunner&& other) {
  task_runner = std::move(other.task_runner);
  mojo_callback = std::move(other.mojo_callback);
  other.task_runner = nullptr;
  other.mojo_callback =
      blink::mojom::DWriteFontProxy::GetUniqueNameLookupTableCallback();
}

DWriteFontLookupTableBuilder::CallbackOnTaskRunner::~CallbackOnTaskRunner() =
    default;

void DWriteFontLookupTableBuilder::QueueShareMemoryRegionWhenReady(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    blink::mojom::DWriteFontProxy::GetUniqueNameLookupTableCallback callback) {
  TRACE_EVENT0("dwrite,fonts",
               "DWriteFontLookupTableBuilder::QueueShareMemoryRegionWhenReady");
  DCHECK(!HasDWriteUniqueFontLookups());
  pending_callbacks_.emplace_back(std::move(task_runner), std::move(callback));
  // Cover for the condition in which the font table becomes ready briefly after
  // a renderer asking for GetUniqueNameLookupTableIfAvailable(), receiving the
  // information that it wasn't ready.
  if (font_table_built_.IsSignaled())
    PostCallbacks();
}

bool DWriteFontLookupTableBuilder::FontUniqueNameTableReady() {
  TRACE_EVENT0("dwrite,fonts",
               "DWriteFontLookupTableBuilder::FontUniqueNameTableReady");
  DCHECK(base::FeatureList::IsEnabled(features::kFontSrcLocalMatching));
  DCHECK(!HasDWriteUniqueFontLookups());
  return font_table_built_.IsSignaled() && IsFontUniqueNameTableValid();
}

void DWriteFontLookupTableBuilder::
    SchedulePrepareFontUniqueNameTableIfNeeded() {
  DCHECK(base::FeatureList::IsEnabled(features::kFontSrcLocalMatching));

  {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    InitializeDirectWrite();
  }

  // Nothing to do if we have API to directly lookup local fonts by unique name
  // (as on Windows 10, IDWriteFactory3 available).
  if (HasDWriteUniqueFontLookups())
    return;

  // Do not schedule indexing if we do not have a profile or temporary directory
  // to store the cached table. This prevents repetitive and redundant scanning
  // when the ContentBrowserClient did not provide a cache directory, as is the
  // case in content_unittests.
  if (TableCacheFilePath().empty())
    return;

  start_time_table_ready_ = base::TimeTicks::Now();
  scanning_error_reasons_.clear();

  scoped_refptr<base::SequencedTaskRunner> results_collection_task_runner =
      base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});

  results_collection_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&DWriteFontLookupTableBuilder::PrepareFontUniqueNameTable,
                     base::Unretained(this)));
}

void DWriteFontLookupTableBuilder::PrepareFontUniqueNameTable() {
  TRACE_EVENT0("dwrite,fonts",
               "DWriteFontLookupTableBuilder::PrepareFontUniqueNameTable");
  DCHECK(!HasDWriteUniqueFontLookups());
  // The table must only be built once.
  DCHECK(!font_table_built_.IsSignaled());

  if (caching_enabled_ && LoadFromFile()) {
    blink::FontUniqueNameTable font_table;
    const bool update_needed =
        !IsFontUniqueNameTableValid() ||
        !font_table.ParseFromArray(font_table_memory_.mapping.memory(),
                                   font_table_memory_.mapping.size()) ||
        font_table.stored_for_platform_version_identifier() !=
            ComputePersistenceHash();

    UMA_HISTOGRAM_BOOLEAN("DirectWrite.Fonts.Proxy.LookupTableDiskCacheHit",
                          !update_needed);
    if (!update_needed) {
      base::TimeDelta duration =
          base::TimeTicks::Now() - start_time_table_ready_;
      UMA_HISTOGRAM_MEDIUM_TIMES("DirectWrite.Fonts.Proxy.LookupTableReadyTime",
                                 duration);
      font_table_built_.Signal();
      PostCallbacks();
      return;
    }
  }

  start_time_table_build_ = base::TimeTicks::Now();
  font_unique_name_table_ = std::make_unique<blink::FontUniqueNameTable>();

  // The |stored_for_platform_version_identifier| proto field is used for
  // persisting the table to disk and identifying whether an update to the
  // table is needed when loading it back.
  font_unique_name_table_->set_stored_for_platform_version_identifier(
      ComputePersistenceHash());

  {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);

    outstanding_family_results_ = collection_->GetFontFamilyCount();
    family_results_empty_ = 0;
    family_results_non_empty_ = 0;
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "DirectWrite.Fonts.Proxy.FamilyCountIndexingStart",
        outstanding_family_results_, 1, 5000, 50);
  }
  for (UINT32 family_index = 0; family_index < outstanding_family_results_;
       ++family_index) {
    // Specify base::ThreadPolicy::MUST_USE_FOREGROUND because in
    // https://crbug.com/960263 we observed a priority inversion when running
    // DWrite worker tasks in the background.
    base::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::ThreadPolicy::MUST_USE_FOREGROUND,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(
            &ExtractPathAndNamesFromFamily, collection_, family_index,
            start_time_table_build_, slow_down_mode_for_testing_,
            OptionalOrNullptr(hang_event_for_testing_), IndexingTimeout()),
        base::BindOnce(&DWriteFontLookupTableBuilder::
                           AppendFamilyResultAndFinalizeIfNeeded,
                       base::Unretained(this)));
  }
  // Post a task to catch timeouts should one of the
  // tasks will eventually not reply.
  timeout_callback_.Reset(base::BindOnce(
      &DWriteFontLookupTableBuilder::OnTimeout, base::Unretained(this)));
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, timeout_callback_.callback(), IndexingTimeout());
}

// static
DWriteFontLookupTableBuilder::FamilyResult
DWriteFontLookupTableBuilder::ExtractPathAndNamesFromFamily(
    Microsoft::WRL::ComPtr<IDWriteFontCollection> collection,
    uint32_t family_index,
    base::TimeTicks start_time,
    SlowDownMode slow_down_mode_for_testing,
    base::WaitableEvent* hang_event_for_testing,
    base::TimeDelta indexing_timeout) {
  TRACE_EVENT0("dwrite,fonts",
               "DWriteFontLookupTableBuilder::ExtractPathAndNamesFromFamily");

  static base::NoDestructor<base::string16> windows_fonts_path(
      GetWindowsFontsPath());

  DWriteFontLookupTableBuilder::FamilyResult family_result;

  if (base::TimeTicks::Now() - start_time > indexing_timeout) {
    family_result.exit_hresult = kErrorFontScanningTimedOut;
    return family_result;
  }

  Microsoft::WRL::ComPtr<IDWriteFontFamily> family;
  HRESULT hr = collection->GetFontFamily(family_index, &family);
  if (FAILED(hr)) {
    family_result.exit_hresult = hr;
    return family_result;
  }
  UINT32 font_count = family->GetFontCount();

  HRESULT last_hresult_continue_reason = S_OK;
  for (UINT32 font_index = 0; font_index < font_count; ++font_index) {
    if (base::TimeTicks::Now() - start_time > indexing_timeout) {
      family_result.exit_hresult = kErrorFontScanningTimedOut;
      return family_result;
    }

    Microsoft::WRL::ComPtr<IDWriteFont> font;
    {
      base::ScopedBlockingCall scoped_blocking_call(
          FROM_HERE, base::BlockingType::MAY_BLOCK);
      hr = family->GetFont(font_index, &font);
    }
    if (FAILED(hr)) {
      family_result.exit_hresult = hr;
      return family_result;
    }

    if (font->GetSimulations() != DWRITE_FONT_SIMULATIONS_NONE)
      continue;

    std::set<base::string16> path_set;
    std::set<base::string16> custom_font_path_set;
    uint32_t ttc_index = 0;
    {
      base::ScopedBlockingCall scoped_blocking_call(
          FROM_HERE, base::BlockingType::MAY_BLOCK);
      hr = AddFilesForFont(font.Get(), *windows_fonts_path, &path_set,
                           &custom_font_path_set, &ttc_index);
      if (FAILED(hr)) {
        // It's possible to not be able to retrieve a font file for a font that
        // is in the system font collection, see https://crbug.com/922183. If we
        // were not able to retrieve a file for a registered font, we do not
        // need to add it to the map.
        last_hresult_continue_reason = hr;
        continue;
      }
    }

    // After having received clarification from Microsoft, the API is designed
    // for allowing multiple files to be returned, if MS was to support a file
    // format like Type1 fonts with this API, but for now only ever returns 1
    // font file as only TrueType / OpenType fonts are supported.
    DCHECK_EQ(path_set.size() + custom_font_path_set.size(), 1u);
    // If this font is placed in a custom font path location, we pass it to
    // Blink, and we'll track with UMA there if such a font path is matched
    // and used. If this happens more than very rarely, we will need to add an
    // out-of-process loading mechanism for loading those uniquely matched
    // font files.
    base::FilePath file_path(path_set.size() ? *path_set.begin()
                                             : *custom_font_path_set.begin());
    DCHECK(!file_path.empty());

    // Build entry for being added to the table in separate call.
    blink::FontUniqueNameTable_UniqueFont unique_font;
    unique_font.set_file_path(file_path.AsUTF8Unsafe());
    unique_font.set_ttc_index(ttc_index);

    std::vector<std::string> extracted_names;
    auto extract_names =
        [&extracted_names,
         &font](DWRITE_INFORMATIONAL_STRING_ID font_info_string_id) -> HRESULT {
      // Now get names, and make them point to the added font.
      Microsoft::WRL::ComPtr<IDWriteLocalizedStrings> font_id_keyed_names;
      BOOL has_id_keyed_names;
      {
        base::ScopedBlockingCall scoped_blocking_call(
            FROM_HERE, base::BlockingType::MAY_BLOCK);
        HRESULT hr = font->GetInformationalStrings(
            font_info_string_id, &font_id_keyed_names, &has_id_keyed_names);
        if (FAILED(hr))
          return hr;
        if (!has_id_keyed_names)
          return kErrorNoFullNameOrPostScriptName;
      }

      return ExtractCaseFoldedLocalizedStrings(font_id_keyed_names.Get(),
                                               &extracted_names)
                 ? S_OK
                 : kErrorExtractingLocalizedStringsFailed;
    };

    hr = extract_names(DWRITE_INFORMATIONAL_STRING_POSTSCRIPT_NAME);
    hr = FAILED(hr) ? hr : extract_names(DWRITE_INFORMATIONAL_STRING_FULL_NAME);

    if (UNLIKELY(slow_down_mode_for_testing == SlowDownMode::kDelayEachTask)) {
      base::PlatformThread::Sleep(indexing_timeout *
                                  kIndexingSlowDownForTestingPercentage);
    } else if (UNLIKELY(slow_down_mode_for_testing ==
                        SlowDownMode::kHangOneTask) &&
               family_index == 0) {
      base::ScopedAllowBaseSyncPrimitivesForTesting scoped_allow_sync_;
      DCHECK(hang_event_for_testing);
      hang_event_for_testing->Wait();
    }

    if (extracted_names.empty()) {
      last_hresult_continue_reason = hr;
      continue;
    }

    family_result.exit_hresult = S_OK;
    family_result.font_files_with_names.push_back(
        DWriteFontLookupTableBuilder::FontFileWithUniqueNames(
            std::move(unique_font), std::move(extracted_names)));
  }

  if (family_result.font_files_with_names.empty()) {
    family_result.exit_hresult = last_hresult_continue_reason;
  }

  return family_result;
}

void DWriteFontLookupTableBuilder::AppendFamilyResultAndFinalizeIfNeeded(
    const FamilyResult& family_result) {
  TRACE_EVENT0(
      "dwrite,fonts",
      "DWriteFontLookupTableBuilder::AppendFamilyResultAndFinalizeIfNeeded");

  outstanding_family_results_--;

  // If this task's response came late and OnTimeout was called, we
  // do not need the results anymore and the table was already finalized.
  if (font_table_built_.IsSignaled())
    return;

  if (!family_result.font_files_with_names.size())
    family_results_empty_++;
  else
    family_results_non_empty_++;

  if (FAILED(family_result.exit_hresult))
    scanning_error_reasons_[family_result.exit_hresult]++;

  for (const FontFileWithUniqueNames& font_of_family :
       family_result.font_files_with_names) {
    blink::FontUniqueNameTable_UniqueFont* added_unique_font =
        font_unique_name_table_->add_fonts();

    *added_unique_font = font_of_family.font_entry;

    int added_font_index = font_unique_name_table_->fonts_size() - 1;

    for (auto& font_name : font_of_family.extracted_names) {
      blink::FontUniqueNameTable_UniqueNameToFontMapping* added_mapping =
          font_unique_name_table_->add_name_map();
      DCHECK(added_mapping);
      added_mapping->set_font_name(font_name);
      added_mapping->set_font_index(added_font_index);
    }
  }

  if (!outstanding_family_results_) {
    FinalizeFontTable();
  }
}

void DWriteFontLookupTableBuilder::FinalizeFontTable() {
  TRACE_EVENT0("dwrite,fonts",
               "DWriteFontLookupTableBuilder::FinalizeFontTable");
  DCHECK(!font_table_built_.IsSignaled());

  timeout_callback_.Cancel();

  // Make sure that whatever happens in the remainder of this function the
  // FontUniqueNameTable object gets released by moving it to a local variable.
  std::unique_ptr<blink::FontUniqueNameTable> font_unique_name_table(
      std::move(font_unique_name_table_));

  bool timed_out = false;
  if (base::TimeTicks::Now() - start_time_table_build_ > IndexingTimeout()) {
    font_unique_name_table->clear_fonts();
    font_unique_name_table->clear_name_map();
    timed_out = true;
  }
  UMA_HISTOGRAM_BOOLEAN("DirectWrite.Fonts.Proxy.TableBuildTimedOut",
                        timed_out);

  int empty_family_results_percentage =
      round(((family_results_empty_ * 1.0f) /
             (family_results_empty_ + family_results_non_empty_)) *
            100.0);
  UMA_HISTOGRAM_PERCENTAGE("DirectWrite.Fonts.Proxy.EmptyFamilyResultsRatio",
                           empty_family_results_percentage);

  if (empty_family_results_percentage > 0) {
    auto most_frequent_hresult_element = std::max_element(
        std::begin(scanning_error_reasons_), std::end(scanning_error_reasons_),
        [](const decltype(scanning_error_reasons_)::value_type& a,
           decltype(scanning_error_reasons_)::value_type& b) {
          return a.second < b.second;
        });
    base::UmaHistogramSparse(
        "DirectWrite.Fonts.Proxy.MostFrequentScanningFailure",
        most_frequent_hresult_element->first);
  }

  unsigned num_font_files = font_unique_name_table->fonts_size();

  blink::FontTableMatcher::SortUniqueNameTableForSearch(
      font_unique_name_table.get());

  font_table_memory_ = base::ReadOnlySharedMemoryRegion::Create(
      font_unique_name_table->ByteSizeLong());

  if (!IsFontUniqueNameTableValid() ||
      !font_unique_name_table->SerializeToArray(
          font_table_memory_.mapping.memory(),
          font_table_memory_.mapping.size())) {
    font_table_memory_ = base::MappedReadOnlyRegion();
  }

  if (caching_enabled_) {
    bool persist_succeeded = PersistToFile();
    UMA_HISTOGRAM_BOOLEAN("DirectWrite.Fonts.Proxy.LookupTablePersistSuccess",
                          persist_succeeded);
  }

  font_table_built_.Signal();
  PostCallbacks();

  if (!IsFontUniqueNameTableValid())
    return;

  base::TimeDelta duration = base::TimeTicks::Now() - start_time_table_build_;
  UMA_HISTOGRAM_MEDIUM_TIMES("DirectWrite.Fonts.Proxy.LookupTableBuildTime",
                             duration);

  duration = base::TimeTicks::Now() - start_time_table_ready_;
  UMA_HISTOGRAM_MEDIUM_TIMES("DirectWrite.Fonts.Proxy.LookupTableReadyTime",
                             duration);

  // The size is usually tens of kilobytes, ~50kb on a standard Windows 10
  // installation, 1MB should be a more than high enough upper limit.
  UMA_HISTOGRAM_CUSTOM_COUNTS("DirectWrite.Fonts.Proxy.LookupTableSize",
                              font_table_memory_.mapping.size() / 1024, 1, 1000,
                              50);

  UMA_HISTOGRAM_CUSTOM_COUNTS("DirectWrite.Fonts.Proxy.NumFontFiles",
                              num_font_files, 1, 5000, 50);

  UMA_HISTOGRAM_CUSTOM_COUNTS("DirectWrite.Fonts.Proxy.IndexingSpeed",
                              num_font_files / duration.InSecondsF(), 1, 10000,
                              50);
}

void DWriteFontLookupTableBuilder::OnTimeout() {
  DCHECK(!font_table_built_.IsSignaled());
  FinalizeFontTable();
}

void DWriteFontLookupTableBuilder::SetSlowDownIndexingForTestingWithTimeout(
    SlowDownMode slow_down_mode,
    base::TimeDelta new_timeout) {
  slow_down_mode_for_testing_ = slow_down_mode;
  font_indexing_timeout_ = new_timeout;
  if (slow_down_mode == SlowDownMode::kHangOneTask)
    hang_event_for_testing_.emplace();
}

void DWriteFontLookupTableBuilder::ResetLookupTableForTesting() {
  slow_down_mode_for_testing_ = SlowDownMode::kNoSlowdown;
  font_indexing_timeout_ = kFontIndexingTimeoutDefault;
  font_table_memory_ = base::MappedReadOnlyRegion();
  caching_enabled_ = true;
  font_table_built_.Reset();
}

void DWriteFontLookupTableBuilder::ResetStateForTesting() {
  ResetLookupTableForTesting();
  // Recreate fFactory3 if available, to reset
  // OverrideDWriteVersionChecksForTesting().
  direct_write_initialized_ = false;
  InitializeDirectWrite();
  InitializeCacheDirectoryFromProfile();
}

void DWriteFontLookupTableBuilder::ResumeFromHangForTesting() {
  hang_event_for_testing_->Signal();
}

// static
DWriteFontLookupTableBuilder* DWriteFontLookupTableBuilder::GetInstance() {
  static base::NoDestructor<DWriteFontLookupTableBuilder> instance;
  return instance.get();
}

}  // namespace content
