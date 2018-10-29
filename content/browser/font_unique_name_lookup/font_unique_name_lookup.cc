// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/font_unique_name_lookup/font_unique_name_lookup.h"

#include "base/android/build_info.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
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

bool SfntNameIsEnglish(const FT_SfntName& sfnt_name) {
  if (sfnt_name.platform_id == TT_PLATFORM_MICROSOFT)
    return sfnt_name.language_id == TT_MS_LANGID_ENGLISH_UNITED_STATES;
  if (sfnt_name.platform_id == TT_PLATFORM_MACINTOSH)
    return sfnt_name.language_id == TT_MAC_LANGID_ENGLISH;
  return false;
}

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

}  // namespace

namespace content {

class PlatformFontUniqueNameLookup : public FontUniqueNameLookup {
 public:
  PlatformFontUniqueNameLookup() : FontUniqueNameLookup(GetCacheDirectory()) {
    // Error from LoadFromFile() is ignored: Loading the cache file could be
    // recovered from by rebuilding the font table. UpdateTableIfNeeded() checks
    // whether the internal base::MappedReadOnlyRegion has a size, which it
    // doesn't if the LoadFromFile() failed. If it doesn't have a size, the
    // table is rebuild by calling UpdateTable().
    LoadFromFile();
    if (UpdateTableIfNeeded()) {
      // TODO(drott): Add UMA histograms for recording cache read and write
      // failures.
      PersistToFile();
    }
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

FontUniqueNameLookup::FontUniqueNameLookup(FontUniqueNameLookup&&) = default;

FontUniqueNameLookup::FontUniqueNameLookup(
    const base::FilePath& cache_directory)
    : cache_directory_(cache_directory) {
  if (!DirectoryExists(cache_directory_) ||
      !base::PathIsWritable(cache_directory_)) {
    DCHECK(false) << "Error accessing cache directory for writing: "
                  << cache_directory_.value();
    cache_directory_ = base::FilePath();
  }
  FT_Init_FreeType(&ft_library_);
}

FontUniqueNameLookup::~FontUniqueNameLookup() {
  FT_Done_FreeType(ft_library_);
}

base::ReadOnlySharedMemoryRegion
FontUniqueNameLookup::GetUniqueNameTableAsSharedMemoryRegion() const {
  return proto_storage_.region.Duplicate();
}

bool FontUniqueNameLookup::IsValid() {
  return proto_storage_.IsValid() && proto_storage_.mapping.size();
}

bool FontUniqueNameLookup::UpdateTableIfNeeded() {
  blink::FontUniqueNameTable font_table;
  bool update_needed =
      !proto_storage_.IsValid() || !proto_storage_.mapping.size() ||
      !font_table.ParseFromArray(proto_storage_.mapping.memory(),
                                 proto_storage_.mapping.size()) ||
      font_table.stored_for_android_build_fp() != GetAndroidBuildFingerprint();
  if (update_needed)
    UpdateTable();
  return update_needed;
}

bool FontUniqueNameLookup::UpdateTable() {
  std::vector<std::string> font_files_to_index = GetFontFilePaths();

  blink::FontUniqueNameTable font_table;
  font_table.set_stored_for_android_build_fp(GetAndroidBuildFingerprint());
  for (const auto& font_file : font_files_to_index) {
    int32_t number_of_faces = NumberOfFacesInFontFile(font_file);
    for (int32_t i = 0; i < number_of_faces; ++i) {
      if (!IndexFile(font_table.add_font_entries(), font_file, i)) {
        // TODO(drott): Track file scanning failures in UMA.
        font_table.mutable_font_entries()->RemoveLast();
      }
    }
  }

  proto_storage_ =
      base::ReadOnlySharedMemoryRegion::Create(font_table.ByteSizeLong());
  if (!IsValid())
    return false;

  if (!font_table.SerializeToArray(proto_storage_.mapping.memory(),
                                   proto_storage_.mapping.size())) {
    proto_storage_ = base::MappedReadOnlyRegion();
    return false;
  }
  return true;
}

bool FontUniqueNameLookup::LoadFromFile() {
  // Reset to empty to ensure IsValid() is false if reading fails.
  proto_storage_ = base::MappedReadOnlyRegion();
  base::File table_cache_file(
      TableCacheFilePath(),
      base::File::FLAG_OPEN | base::File::Flags::FLAG_READ);
  if (!table_cache_file.IsValid())
    return false;
  proto_storage_ =
      base::ReadOnlySharedMemoryRegion::Create(table_cache_file.GetLength());
  if (!IsValid())
    return false;
  int read_result = table_cache_file.Read(
      0, static_cast<char*>(proto_storage_.mapping.memory()),
      table_cache_file.GetLength());
  // If no bytes were read or Read() returned -1 we are not able to reconstruct
  // a font table from the cached file.
  if (read_result <= 0) {
    proto_storage_ = base::MappedReadOnlyRegion();
    return false;
  }

  blink::FontUniqueNameTable font_table;
  if (!font_table.ParseFromArray(proto_storage_.mapping.memory(),
                                 proto_storage_.mapping.size())) {
    proto_storage_ = base::MappedReadOnlyRegion();
    return false;
  }

  return true;
}

bool FontUniqueNameLookup::PersistToFile() {
  DCHECK(IsValid());
  if (!IsValid())
    return false;
  base::File table_cache_file(
      TableCacheFilePath(),
      base::File::FLAG_CREATE_ALWAYS | base::File::Flags::FLAG_WRITE);
  if (!table_cache_file.IsValid())
    return false;
  if (table_cache_file.Write(
          0, static_cast<char*>(proto_storage_.mapping.memory()),
          proto_storage_.mapping.size()) == -1) {
    table_cache_file.SetLength(0);
    proto_storage_ = base::MappedReadOnlyRegion();
    return false;
  }
  return true;
}

base::FilePath FontUniqueNameLookup::TableCacheFilePath() {
  return base::FilePath(
      cache_directory_.Append(base::FilePath(kProtobufFilename)));
}

bool FontUniqueNameLookup::IndexFile(
    blink::FontUniqueNameTable_FontUniqueNameEntry* font_entry,
    const std::string& font_file_path,
    uint32_t ttc_index) {
  ScopedFtFace face(ft_library_, font_file_path.c_str(), ttc_index);
  if (!face.IsValid()) {
    LOG(ERROR) << "Unable to open font file for indexing: "
               << font_file_path.c_str()
               << " - FreeType FT_Error code: " << face.error();
    return false;
  }

  if (!FT_Get_Sfnt_Name_Count(face.get())) {
    LOG(ERROR) << "Zero name table entries in font file: "
               << font_file_path.c_str();
    return false;
  }

  // Get file attributes
  base::File font_file_for_info(
      base::FilePath(font_file_path.c_str()),
      base::File::FLAG_OPEN | base::File::Flags::FLAG_READ);
  if (!font_file_for_info.IsValid()) {
    LOG(ERROR) << "Unable to open font file: " << font_file_path.c_str();
    return false;
  }
  base::File::Info font_file_info;
  if (!font_file_for_info.GetInfo(&font_file_info)) {
    LOG(ERROR) << "Unable to get font file attributes for: "
               << font_file_path.c_str();
    return false;
  }
  font_entry->set_file_path(font_file_path);
  font_entry->set_ttc_index(ttc_index);

  for (size_t i = 0; i < FT_Get_Sfnt_Name_Count(face.get()); ++i) {
    FT_SfntName sfnt_name;
    if (FT_Get_Sfnt_Name(face.get(), i, &sfnt_name) != 0) {
      LOG(ERROR) << "Unable to retrieve Sfnt Name table for font file: "
                 << font_file_path.c_str();
      return false;
    }

    if (!SfntNameIsEnglish(sfnt_name))
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
    if (sfnt_name_unicode.isBogus())
      return false;
    // Firefox performs case insensitive matching for src: local().
    sfnt_name_unicode.foldCase();
    sfnt_name_unicode.toUTF8String(sfnt_name_string);

    switch (sfnt_name.name_id) {
      case TT_NAME_ID_PS_NAME:
        font_entry->set_postscript_name(blink::IcuFoldCase(sfnt_name_string));
        break;
      case TT_NAME_ID_FULL_NAME:
        font_entry->set_full_name(blink::IcuFoldCase(sfnt_name_string));
        break;
      default:
        break;
    }
  }
  return true;
}

int32_t FontUniqueNameLookup::NumberOfFacesInFontFile(
    const std::string& font_filename) const {
  // According to FreeType documentation calling FT_Open_Face with a negative
  // index value allows us to probe how many fonts can be found in a font file
  // (which can be a single font ttf or a TrueType collection (.ttc).
  ScopedFtFace probe_face(ft_library_, font_filename.c_str(), -1);
  if (!probe_face.IsValid())
    return 0;
  return probe_face.get()->num_faces;
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

}  // namespace content
