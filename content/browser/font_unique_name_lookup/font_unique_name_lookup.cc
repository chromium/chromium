// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/font_unique_name_lookup/font_unique_name_lookup.h"

#include "base/android/build_info.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/public/common/font_unique_name_lookup/font_table_matcher.h"
#include "third_party/blink/public/common/font_unique_name_lookup/font_table_persistence.h"
#include "third_party/blink/public/common/font_unique_name_lookup/font_unique_name_table.pb.h"
#include "third_party/blink/public/common/font_unique_name_lookup/icu_fold_case_util.h"

#include <set>
#include <vector>
#include "third_party/icu/source/common/unicode/unistr.h"

#include FT_TRUETYPE_IDS_H

namespace {

const char kProtobufFilename[] = "font_unique_name_table.pb";
static const char* const kAndroidFontPaths[] = {"/system/fonts",
                                                "/vendor/fonts"};

// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "FontScanningResult" in src/tools/metrics/histograms/enums.xml.
enum class FontScanningResult {
  kSuccess = 0,
  kFtNewFaceFailed = 1,
  kZeroNameTableEntries = 2,
  kUnableToRetriveNameEntry = 3,
  kNameInvalidUnicode = 4,
  kMaxValue = kNameInvalidUnicode
};

void LogUMAFontScanningResult(FontScanningResult result) {
  UMA_HISTOGRAM_ENUMERATION("Blink.Fonts.AndroidFontScanningResult", result);
}

void LogUMAPersistSuccess(bool success) {
  UMA_HISTOGRAM_BOOLEAN("Blink.Fonts.AndroidFontScanningPersistToFileSuccess",
                        success);
}

void LogUMALoadFromFileSuccess(bool success) {
  UMA_HISTOGRAM_BOOLEAN("Blink.Fonts.AndroidFontScanningLoadFromFileSuccess",
                        success);
}

void LogUMAFontScanningUpdateNeeded(bool update_needed) {
  UMA_HISTOGRAM_BOOLEAN("Blink.Fonts.AndroidFontScanningUpdateNeeded",
                        update_needed);
}

void LogUMAFontScanningDuration(base::TimeDelta duration) {
  UMA_HISTOGRAM_MEDIUM_TIMES("Blink.Fonts.AndroidFontScanningTableBuildTime",
                             duration);
}

void LogUMALookupTableReadyDuration(base::TimeDelta duration) {
  UMA_HISTOGRAM_MEDIUM_TIMES("Blink.Fonts.AndroidFontLookupTableReadyTime",
                             duration);
}

void LogUMALookupTableLoadFromFileDuration(base::TimeDelta duration) {
  UMA_HISTOGRAM_MEDIUM_TIMES("Blink.Fonts.AndroidFontLookupLoadFromFileTime",
                             duration);
}

bool IsRelevantNameRecord(const FT_SfntName& sfnt_name) {
  if (sfnt_name.name_id != TT_NAME_ID_FULL_NAME &&
      sfnt_name.name_id != TT_NAME_ID_PS_NAME)
    return false;

  // From the CSS Fonts spec chapter 4.3. Font reference: the src descriptor
  // "For OpenType fonts with multiple localizations of the full font name,
  // the US English version is used (language ID = 0x409 for Windows and
  // language ID = 0 for Macintosh) or the first localization when a US
  // English full font name is not available (the OpenType specification
  // recommends that all fonts minimally include US English names)."
  // Since we can assume Android system fonts contain an English name,
  // continue here.
  if (sfnt_name.platform_id == TT_PLATFORM_MICROSOFT)
    return sfnt_name.language_id == TT_MS_LANGID_ENGLISH_UNITED_STATES;

  if (sfnt_name.platform_id == TT_PLATFORM_MACINTOSH)
    return sfnt_name.language_id == TT_MAC_LANGID_ENGLISH;

  return false;
}

// Scoped wrapper for a FreeType library object in order to ensure
// initialization and tear down. Used during scanning font files.
class ScopedFtLibrary {
 public:
  ScopedFtLibrary() { FT_Init_FreeType(&ft_library_); }

  ~ScopedFtLibrary() { FT_Done_FreeType(ft_library_); }

  FT_Library get() { return ft_library_; }

 private:
  FT_Library ft_library_;
};

// Convenience scoped wrapper for FT_Face instances. Takes care of handling
// FreeType memory by calling FT_Done_Face on destruction.
class ScopedFtFace {
 public:
  // Create a new FT_Face instance that will be wrapped by this object.
  // Call IsValid() after construction to check for errors.
  // |library| is the parent FT_Library instance, |font_path| the input font
  // file path, and |ttc_index| the font file index (for TrueType collections).
  ScopedFtFace(FT_Library library,
               const std::string& font_path,
               int32_t ttc_index)
      : ft_face_(nullptr),
        ft_error_(
            FT_New_Face(library, font_path.c_str(), ttc_index, &ft_face_)) {}

  // Destructor will destroy the FT_Face instance automatically.
  ~ScopedFtFace() {
    if (IsValid()) {
      FT_Done_Face(ft_face_);
    }
  }

  // Returns true iff instance is valid, i.e. construction did not fail.
  bool IsValid() const { return ft_error_ == FT_Err_Ok; }

  // Return FreeType error code from construction.
  FT_Error error() const { return ft_error_; }

  // Returns FT_Face value.
  FT_Face get() const { return ft_face_; }

 private:
  FT_Face ft_face_ = nullptr;
  FT_Error ft_error_ = FT_Err_Ok;
};

void IndexFile(FT_Library ft_library,
               blink::FontUniqueNameTable* font_table,
               const std::string& font_file_path,
               uint32_t ttc_index) {
  ScopedFtFace face(ft_library, font_file_path.c_str(), ttc_index);
  if (!face.IsValid()) {
    LogUMAFontScanningResult(FontScanningResult::kFtNewFaceFailed);
    return;
  }

  if (!FT_Get_Sfnt_Name_Count(face.get())) {
    LogUMAFontScanningResult(FontScanningResult::kZeroNameTableEntries);
    return;
  }

  blink::FontUniqueNameTable_UniqueFont* added_unique_font =
      font_table->add_fonts();
  added_unique_font->set_file_path(font_file_path);
  added_unique_font->set_ttc_index(ttc_index);

  int added_font_index = font_table->fonts_size() - 1;

  for (size_t i = 0; i < FT_Get_Sfnt_Name_Count(face.get()); ++i) {
    FT_SfntName sfnt_name;
    if (FT_Get_Sfnt_Name(face.get(), i, &sfnt_name) != 0) {
      LogUMAFontScanningResult(FontScanningResult::kUnableToRetriveNameEntry);
      return;
    }

    if (!IsRelevantNameRecord(sfnt_name))
      continue;

    std::string sfnt_name_string = "";
    std::string codepage_name;
    // Codepage names from http://demo.icu-project.org/icu-bin/convexp
    if (sfnt_name.platform_id == TT_PLATFORM_MICROSOFT &&
        sfnt_name.encoding_id == TT_MS_ID_UNICODE_CS) {
      codepage_name = "UTF16-BE";
    } else if (sfnt_name.platform_id == TT_PLATFORM_MACINTOSH &&
               sfnt_name.encoding_id == TT_MAC_ID_ROMAN) {
      codepage_name = "macintosh";
    }
    icu::UnicodeString sfnt_name_unicode(
        reinterpret_cast<char*>(sfnt_name.string), sfnt_name.string_len,
        codepage_name.c_str());
    if (sfnt_name_unicode.isBogus()) {
      LogUMAFontScanningResult(FontScanningResult::kNameInvalidUnicode);
      return;
    }
    // Firefox performs case insensitive matching for src: local().
    sfnt_name_unicode.foldCase();
    sfnt_name_unicode.toUTF8String(sfnt_name_string);

    blink::FontUniqueNameTable_UniqueNameToFontMapping* name_mapping =
        font_table->add_name_map();
    name_mapping->set_font_name(blink::IcuFoldCase(sfnt_name_string));
    name_mapping->set_font_index(added_font_index);
  }
  LogUMAFontScanningResult(FontScanningResult::kSuccess);
}

int32_t NumberOfFacesInFontFile(FT_Library ft_library,
                                const std::string& font_filename) {
  // According to FreeType documentation calling FT_Open_Face with a negative
  // index value allows us to probe how many fonts can be found in a font file
  // (which can be a single font ttf or a TrueType collection (.ttc)).
  ScopedFtFace probe_face(ft_library, font_filename.c_str(), -1);
  if (!probe_face.IsValid())
    return 0;
  return probe_face.get()->num_faces;
}

}  // namespace

namespace content {

class PlatformFontUniqueNameLookup : public FontUniqueNameLookup {
 public:
  PlatformFontUniqueNameLookup() : FontUniqueNameLookup(GetCacheDirectory()) {
    ScheduleLoadOrUpdateTable();
  }

 private:
  static base::FilePath GetCacheDirectory() {
    base::FilePath cache_directory;
    base::PathService::Get(base::DIR_CACHE, &cache_directory);
    return cache_directory;
  }
};

FontUniqueNameLookup& FontUniqueNameLookup::GetInstance() {
  static base::NoDestructor<PlatformFontUniqueNameLookup> sInstance;
  return *sInstance.get();
}

FontUniqueNameLookup::FontUniqueNameLookup(
    const base::FilePath& cache_directory)
    : cache_directory_(cache_directory) {
  if (!DirectoryExists(cache_directory_) ||
      !base::PathIsWritable(cache_directory_)) {
    DCHECK(false) << "Error accessing cache directory for writing: "
                  << cache_directory_.value();
    cache_directory_ = base::FilePath();
  }
}

FontUniqueNameLookup::~FontUniqueNameLookup() = default;

base::ReadOnlySharedMemoryRegion FontUniqueNameLookup::DuplicateMemoryRegion() {
  DCHECK(proto_storage_.IsValid() && proto_storage_.mapping.size());
  return proto_storage_.region.Duplicate();
}

void FontUniqueNameLookup::QueueShareMemoryRegionWhenReady(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    blink::mojom::FontUniqueNameLookup::GetUniqueNameLookupTableCallback
        callback) {
  pending_callbacks_.emplace_back(std::move(task_runner), std::move(callback));
}

bool FontUniqueNameLookup::IsValid() {
  return proto_storage_ready_.IsSignaled() && proto_storage_.IsValid() &&
         proto_storage_.mapping.size();
}

bool FontUniqueNameLookup::UpdateTableIfNeeded() {
  TRACE_EVENT0("fonts", "FontUniqueNameLookup::UpdateTableIfNeeded");
  blink::FontUniqueNameTable font_table;
  bool update_needed =
      !proto_storage_.IsValid() || !proto_storage_.mapping.size() ||
      !font_table.ParseFromArray(proto_storage_.mapping.memory(),
                                 proto_storage_.mapping.size()) ||
      font_table.stored_for_platform_version_identifier() !=
          GetAndroidBuildFingerprint();

  LogUMAFontScanningUpdateNeeded(update_needed);
  if (update_needed)
    UpdateTable();
  return update_needed;
}

bool FontUniqueNameLookup::UpdateTable() {
  TRACE_EVENT0("fonts", "FontUniqueNameLookup::UpdateTable");

  base::TimeTicks update_table_start_time = base::TimeTicks::Now();

  std::vector<std::string> font_files_to_index = GetFontFilePaths();

  ScopedFtLibrary ft_library;
  blink::FontUniqueNameTable font_table;
  font_table.set_stored_for_platform_version_identifier(
      GetAndroidBuildFingerprint());
  for (const auto& font_file : font_files_to_index) {
    int32_t number_of_faces =
        NumberOfFacesInFontFile(ft_library.get(), font_file);
    for (int32_t i = 0; i < number_of_faces; ++i) {
      TRACE_EVENT0("fonts", "FontUniqueNameLookup::UpdateTable - IndexFile");
      IndexFile(ft_library.get(), &font_table, font_file, i);
    }
  }

  blink::FontTableMatcher::SortUniqueNameTableForSearch(&font_table);

  proto_storage_ =
      base::ReadOnlySharedMemoryRegion::Create(font_table.ByteSizeLong());
  if (!proto_storage_.IsValid() || !proto_storage_.mapping.size())
    return false;

  if (!font_table.SerializeToArray(proto_storage_.mapping.memory(),
                                   proto_storage_.mapping.size())) {
    proto_storage_ = base::MappedReadOnlyRegion();
    return false;
  }

  base::TimeDelta duration = base::TimeTicks::Now() - update_table_start_time;
  LogUMAFontScanningDuration(duration);

  return true;
}

bool FontUniqueNameLookup::LoadFromFile() {
  TRACE_EVENT0("fonts", "FontUniqueNameLookup::LoadFromFile");
  bool load_success = blink::font_table_persistence::LoadFromFile(
      TableCacheFilePath(), &proto_storage_);
  LogUMALoadFromFileSuccess(load_success);
  return load_success;
}

bool FontUniqueNameLookup::PersistToFile() {
  TRACE_EVENT0("fonts", "FontUniqueNameLookup::PersistToFile");
  bool persist_success = blink::font_table_persistence::PersistToFile(
      proto_storage_, TableCacheFilePath());
  LogUMAPersistSuccess(persist_success);
  return persist_success;
}

void FontUniqueNameLookup::ScheduleLoadOrUpdateTable() {
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(
          [](FontUniqueNameLookup* instance) {
            // Error from LoadFromFile() is ignored:
            // Loading the cache file could be recovered
            // from by rebuilding the font table.
            // UpdateTableIfNeeded() checks whether the
            // internal base::MappedReadOnlyRegion has a
            // size, which it doesn't if the LoadFromFile()
            // failed. If it doesn't have a size, the table
            // is rebuild by calling UpdateTable().
            base::TimeTicks prepare_table_start_time = base::TimeTicks::Now();
            bool loaded_from_file = instance->LoadFromFile();
            if (loaded_from_file) {
              LogUMALookupTableLoadFromFileDuration(base::TimeTicks::Now() -
                                                    prepare_table_start_time);
            }
            if (instance->UpdateTableIfNeeded()) {
              instance->PersistToFile();
            }
            LogUMALookupTableReadyDuration(base::TimeTicks::Now() -
                                           prepare_table_start_time);
            instance->proto_storage_ready_.Signal();
            instance->PostCallbacks();
          },
          base::Unretained(this)));
}

base::FilePath FontUniqueNameLookup::TableCacheFilePath() {
  return base::FilePath(
      cache_directory_.Append(base::FilePath(kProtobufFilename)));
}

std::string FontUniqueNameLookup::GetAndroidBuildFingerprint() const {
  return android_build_fingerprint_for_testing_.size()
             ? android_build_fingerprint_for_testing_
             : base::android::BuildInfo::GetInstance()->android_build_fp();
}

std::vector<std::string> FontUniqueNameLookup::GetFontFilePaths() const {
  if (font_file_paths_for_testing_.size())
    return font_file_paths_for_testing_;
  std::vector<std::string> font_files;
  for (const char* font_dir_path : kAndroidFontPaths) {
    base::FileEnumerator files_enumerator(
        base::MakeAbsoluteFilePath(base::FilePath(font_dir_path)), true,
        base::FileEnumerator::FILES);
    for (base::FilePath name = files_enumerator.Next(); !name.empty();
         name = files_enumerator.Next()) {
      if (name.Extension() == ".ttf" || name.Extension() == ".ttc" ||
          name.Extension() == ".otf") {
        font_files.push_back(name.value());
      }
    }
  }
  return font_files;
}

FontUniqueNameLookup::CallbackOnTaskRunner::CallbackOnTaskRunner(
    scoped_refptr<base::SequencedTaskRunner> runner,
    blink::mojom::FontUniqueNameLookup::GetUniqueNameLookupTableCallback
        callback)
    : task_runner(std::move(runner)), mojo_callback(std::move(callback)) {}

FontUniqueNameLookup::CallbackOnTaskRunner::CallbackOnTaskRunner(
    CallbackOnTaskRunner&& other) {
  task_runner = std::move(other.task_runner);
  mojo_callback = std::move(other.mojo_callback);
  other.task_runner = nullptr;
  other.mojo_callback =
      blink::mojom::FontUniqueNameLookup::GetUniqueNameLookupTableCallback();
}

FontUniqueNameLookup::CallbackOnTaskRunner::~CallbackOnTaskRunner() = default;

void FontUniqueNameLookup::PostCallbacks() {
  for (auto& pending_callback : pending_callbacks_) {
    pending_callback.task_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(pending_callback.mojo_callback),
                                  DuplicateMemoryRegion()));
  }
  pending_callbacks_.clear();
}

}  // namespace content
