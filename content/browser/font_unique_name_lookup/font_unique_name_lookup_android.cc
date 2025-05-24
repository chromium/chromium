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

void IndexFile(blink::FontUniqueNameTable& font_table,
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

void IndexFiles(base::span<base::FilePath> fonts_to_index,
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
      TRACE_EVENT0("fonts", "FontUniqueNameLookup::UpdateTable - IndexFile");
      IndexFile(font_table, font_file_path.value(), mapped_bytes, ttc_index);
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

  IndexFiles(font_files_to_index, font_table);

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
