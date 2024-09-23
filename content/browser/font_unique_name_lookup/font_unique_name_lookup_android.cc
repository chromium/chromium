// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/font_unique_name_lookup/font_unique_name_lookup_android.h"

#include <set>
#include <vector>

#include "base/android/build_info.h"
#include "base/check.h"
#include "base/containers/span_rust.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/cstring_view.h"
#include "base/strings/string_view_rust.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/browser/font_unique_name_lookup/name_table_ffi.rs.h"
#include "content/common/features.h"
#include "third_party/blink/public/common/font_unique_name_lookup/font_table_matcher.h"
#include "third_party/blink/public/common/font_unique_name_lookup/font_table_persistence.h"
#include "third_party/blink/public/common/font_unique_name_lookup/font_unique_name_table.pb.h"
#include "third_party/blink/public/common/font_unique_name_lookup/icu_fold_case_util.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/rust/cxx/v1/cxx.h"

// clang-format off
#include <ft2build.h>
#include FT_SYSTEM_H
#include FT_TRUETYPE_TABLES_H
#include FT_SFNT_NAMES_H
#include FT_TRUETYPE_IDS_H
// clang-format on

static_assert(BUILDFLAG(IS_ANDROID), "This implementation only works safely "
              "on Android due to the way it assumes font files to be "
              "read-only and unmodifiable.");

namespace {

// Increment this suffix when changes are needed to the cache structure, e.g.
// counting up after the dash "-1", "-2", etc.
const char kFingerprintSuffixForceUpdateCache[] = "-2";
const char kProtobufFilename[] = "font_unique_name_table.pb";

// These directories contain read-only font files stored in ROM.
// Memory-mapping these files avoids large RAM allocations.
// DO NOT add directories here unless the files are guaranteed read-only.
// Modifying these files typically requires a firmware or system update.
static const char* const kAndroidFontPaths[] = {
    "/system/fonts", "/vendor/fonts", "/product/fonts"};

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
               base::cstring_view font_path,
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

void IndexFileFreeType(FT_Library ft_library,
                       blink::FontUniqueNameTable& font_table,
                       base::cstring_view font_file_path,
                       uint32_t ttc_index) {
  ScopedFtFace face(ft_library, font_file_path, ttc_index);

  if (!face.IsValid() || !FT_Get_Sfnt_Name_Count(face.get()))
    return;

  blink::FontUniqueNameTable_UniqueFont* added_unique_font =
      font_table.add_fonts();
  added_unique_font->set_file_path(std::string(font_file_path));
  added_unique_font->set_ttc_index(ttc_index);

  int added_font_index = font_table.fonts_size() - 1;

  for (size_t i = 0; i < FT_Get_Sfnt_Name_Count(face.get()); ++i) {
    FT_SfntName sfnt_name;
    if (FT_Get_Sfnt_Name(face.get(), i, &sfnt_name) != 0)
      return;

    if (!IsRelevantNameRecord(sfnt_name))
      continue;

    std::string sfnt_name_string;
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
    if (sfnt_name_unicode.isBogus())
      return;

    // Firefox performs case insensitive matching for src: local().
    sfnt_name_unicode.foldCase();
    sfnt_name_unicode.toUTF8String(sfnt_name_string);

    blink::FontUniqueNameTable_UniqueNameToFontMapping* name_mapping =
        font_table.add_name_map();
    name_mapping->set_font_name(blink::IcuFoldCase(sfnt_name_string));
    name_mapping->set_font_index(added_font_index);
  }
}

int32_t NumberOfFacesInFontFileFreeType(FT_Library ft_library,
                                        base::cstring_view font_filename) {
  // According to FreeType documentation calling FT_Open_Face with a negative
  // index value allows us to probe how many fonts can be found in a font file
  // (which can be a single font ttf or a TrueType collection (.ttc)).
  ScopedFtFace probe_face(ft_library, font_filename, -1);
  if (!probe_face.IsValid())
    return 0;
  return probe_face.get()->num_faces;
}

void IndexFilesFreeType(const base::span<base::FilePath> fonts_to_index,
                        blink::FontUniqueNameTable& font_table) {
  ScopedFtLibrary ft_library;
  for (const auto& font_file_name : fonts_to_index) {
    int32_t number_of_faces = NumberOfFacesInFontFileFreeType(
        ft_library.get(), font_file_name.value());
    for (int32_t i = 0; i < number_of_faces; ++i) {
      TRACE_EVENT0("fonts",
                   "FontUniqueNameLookup::UpdateTable - IndexFileFreeType");
      IndexFileFreeType(ft_library.get(), font_table, font_file_name.value(),
                        i);
    }
  }
}

void IndexFileFontations(blink::FontUniqueNameTable& font_table,
                         std::string_view font_file_path,
                         const rust::Slice<const uint8_t>& mapped_bytes,
                         uint32_t ttc_index) {
  rust::Vec<rust::String> english_unique_font_names =
      name_table_access::english_unique_font_names(mapped_bytes, ttc_index);

  if (english_unique_font_names.empty()) {
    return;
  }

  blink::FontUniqueNameTable_UniqueFont* added_unique_font =
      font_table.add_fonts();
  added_unique_font->set_file_path(std::string(font_file_path));
  added_unique_font->set_ttc_index(ttc_index);

  int added_font_index = font_table.fonts_size() - 1;

  for (const rust::String& entry : english_unique_font_names) {
    blink::FontUniqueNameTable_UniqueNameToFontMapping* name_mapping =
        font_table.add_name_map();
    name_mapping->set_font_name(blink::IcuFoldCase(std::string(entry)));
    name_mapping->set_font_index(added_font_index);
  }
}

void IndexFilesFontations(base::span<base::FilePath> fonts_to_index,
                          blink::FontUniqueNameTable& font_table) {
  for (const auto& font_file_path : fonts_to_index) {
    base::MemoryMappedFile mapped_font_file;
    // Files from kAndroidFontPaths are read-only, protected files on Android,
    // only modified by means of a firmware update. At Chrome's lifetime,
    // these files are not modifiable, which makes them safe to memory-map.
    // For details, see discussion in
    // https://crrev.com/c/5677302
    if (!mapped_font_file.Initialize(font_file_path)) {
      continue;
    }
    rust::Slice<const uint8_t> mapped_bytes(
        base::SpanToRustSlice(mapped_font_file.bytes()));
    int32_t number_of_faces =
        name_table_access::indexable_num_fonts(mapped_bytes);
    for (int32_t ttc_index = 0; ttc_index < number_of_faces; ++ttc_index) {
      TRACE_EVENT0("fonts",
                   "FontUniqueNameLookup::UpdateTable - IndexFileFontations");
      IndexFileFontations(font_table, font_file_path.value(), mapped_bytes,
                          ttc_index);
    }
  }
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
  if (proto_storage_.IsValid() && proto_storage_.mapping.size()) {
    blink::FontUniqueNameTable font_table;
    base::span<const uint8_t> mem(proto_storage_.mapping);
    if (font_table.ParseFromArray(mem.data(), mem.size())) {
      if (font_table.stored_for_platform_version_identifier() ==
          GetAndroidBuildFingerprint()) {
        return false;
      }
    }
  }

  UpdateTable();
  return true;
}

bool FontUniqueNameLookup::UpdateTable() {
  TRACE_EVENT0("fonts", "FontUniqueNameLookup::UpdateTable");

  std::vector<base::FilePath> font_files_to_index = GetFontFilePaths();

  blink::FontUniqueNameTable font_table;
  font_table.set_stored_for_platform_version_identifier(
      GetAndroidBuildFingerprint());

  if (base::FeatureList::IsEnabled(features::kFontIndexingFontations)) {
    IndexFilesFontations(font_files_to_index, font_table);
  } else {
    IndexFilesFreeType(font_files_to_index, font_table);
  }

  blink::FontTableMatcher::SortUniqueNameTableForSearch(&font_table);

  proto_storage_ =
      base::ReadOnlySharedMemoryRegion::Create(font_table.ByteSizeLong());
  if (!proto_storage_.IsValid() || !proto_storage_.mapping.size())
    return false;

  base::span<uint8_t> mem(proto_storage_.mapping);
  if (!font_table.SerializeToArray(mem.data(), mem.size())) {
    proto_storage_ = base::MappedReadOnlyRegion();
    return false;
  }

  return true;
}

bool FontUniqueNameLookup::LoadFromFile() {
  TRACE_EVENT0("fonts", "FontUniqueNameLookup::LoadFromFile");
  return blink::font_table_persistence::LoadFromFile(TableCacheFilePath(),
                                                     &proto_storage_);
}

bool FontUniqueNameLookup::PersistToFile() {
  TRACE_EVENT0("fonts", "FontUniqueNameLookup::PersistToFile");
  return blink::font_table_persistence::PersistToFile(proto_storage_,
                                                      TableCacheFilePath());
}

void FontUniqueNameLookup::ScheduleLoadOrUpdateTable() {
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
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
            instance->LoadFromFile();
            if (instance->UpdateTableIfNeeded()) {
              instance->PersistToFile();
            }
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
             : std::string(base::android::BuildInfo::GetInstance()
                               ->android_build_fp()) +
                   std::string(kFingerprintSuffixForceUpdateCache);
}

std::vector<base::FilePath> FontUniqueNameLookup::GetFontFilePaths() const {
  if (font_file_paths_for_testing_.size())
    return font_file_paths_for_testing_;
  std::vector<base::FilePath> font_files;
  for (const char* font_dir_path : kAndroidFontPaths) {
    base::FileEnumerator files_enumerator(
        base::MakeAbsoluteFilePath(base::FilePath(font_dir_path)), true,
        base::FileEnumerator::FILES);
    for (base::FilePath name = files_enumerator.Next(); !name.empty();
         name = files_enumerator.Next()) {
      if (name.Extension() == ".ttf" || name.Extension() == ".ttc" ||
          name.Extension() == ".otf") {
        font_files.push_back(name);
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
