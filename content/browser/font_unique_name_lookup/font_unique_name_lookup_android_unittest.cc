// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/font_unique_name_lookup/font_unique_name_lookup_android.h"

#include <functional>
#include <memory>

#include "base/android/build_info.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/function_ref.h"
#include "base/strings/string_util.h"
#include "content/browser/font_unique_name_lookup/name_table_ffi.rs.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/font_unique_name_lookup/font_table_matcher.h"

namespace {

static const char* const kAndroidFontPaths[] = {"/system/fonts",
                                                "/vendor/fonts"};

// Full font name, postscript name, filename.
static const char* const kRobotoCondensedBoldItalicNames[] = {
    "Roboto Condensed Bold Italic", "RobotoCondensed-BoldItalic",
    "RobotoCondensed-BoldItalic.ttf"};

std::vector<base::FilePath> AndroidFontFilesList() {
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

std::vector<base::FilePath> SplitFontFilesList(
    const std::vector<base::FilePath>& font_files,
    bool return_second_half) {
  CHECK_GT(font_files.size(), 2u);
  auto start_copy = font_files.begin();
  auto end_copy = font_files.begin() + (font_files.size() / 2);
  if (return_second_half) {
    start_copy = end_copy;
    end_copy = font_files.end();
  }
  return std::vector<base::FilePath>(start_copy, end_copy);
}

void TruncateFileToLength(const base::FilePath& file_path,
                          int64_t truncated_length) {
  base::File file_to_truncate(
      file_path, base::File::FLAG_OPEN | base::File::Flags::FLAG_WRITE);

  ASSERT_TRUE(file_to_truncate.IsValid());
  ASSERT_TRUE(file_to_truncate.SetLength(truncated_length));
}

}  // namespace

namespace content {

class FontUniqueNameLookupTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    font_unique_name_lookup_ =
        std::make_unique<FontUniqueNameLookup>(temp_dir_.GetPath());
  }

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<FontUniqueNameLookup> font_unique_name_lookup_;
};

TEST_F(FontUniqueNameLookupTest, TestBuildLookup) {
  ASSERT_TRUE(font_unique_name_lookup_->UpdateTable());
  base::ReadOnlySharedMemoryMapping mapping =
      font_unique_name_lookup_->DuplicateMemoryRegion().Map();
  blink::FontTableMatcher matcher(mapping);
  ASSERT_GT(matcher.AvailableFonts(), 0u);
  ASSERT_TRUE(font_unique_name_lookup_->PersistToFile());
  ASSERT_TRUE(font_unique_name_lookup_->LoadFromFile());
  blink::FontTableMatcher matcher_after_load(
      font_unique_name_lookup_->DuplicateMemoryRegion().Map());
  ASSERT_GT(matcher_after_load.AvailableFonts(), 0u);
}

TEST_F(FontUniqueNameLookupTest, TestHandleFailedRead) {
  ASSERT_FALSE(base::PathExists(
      font_unique_name_lookup_->TableCacheFilePathForTesting()));
  ASSERT_FALSE(font_unique_name_lookup_->LoadFromFile());
  ASSERT_TRUE(font_unique_name_lookup_->UpdateTable());
  base::ReadOnlySharedMemoryMapping mapping =
      font_unique_name_lookup_->DuplicateMemoryRegion().Map();
  blink::FontTableMatcher matcher(mapping);

  // AOSP Android Kitkat has 81 fonts, the Kitkat bot seems to have 74,
  // Marshmallow has 149, Oreo 247. There are other variants that are built
  // with fewer fonts however. Be safer and assume 10 maximum.
  ASSERT_GT(matcher.AvailableFonts(), 10u);
  ASSERT_TRUE(font_unique_name_lookup_->PersistToFile());
  ASSERT_TRUE(base::PathExists(
      font_unique_name_lookup_->TableCacheFilePathForTesting()));
  int64_t file_size;
  ASSERT_TRUE(base::GetFileSize(
      font_unique_name_lookup_->TableCacheFilePathForTesting(), &file_size));
  // For 10 fonts, assume we have at least 256 bytes of data, it's
  // around 30k in practice on Kitkat with 81 fonts.
  ASSERT_GT(file_size, 256);
  ASSERT_TRUE(font_unique_name_lookup_->LoadFromFile());

  // For each truncated size, reading must fail, otherwise we successfully read
  // a truncated protobuf.
  for (int64_t truncated_size = file_size - 1; truncated_size >= 0;
       truncated_size -= file_size) {
    TruncateFileToLength(
        font_unique_name_lookup_->TableCacheFilePathForTesting(),
        truncated_size);
    ASSERT_FALSE(font_unique_name_lookup_->LoadFromFile());
  }
}

TEST_F(FontUniqueNameLookupTest, TestMatchPostScriptName) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() >=
      base::android::SdkVersion::SDK_VERSION_S) {
    // TODO(crbug.com/40203471): Fonts identified by
    // kRobotoCondensedBoldItalicNames do not seem to be available on Android
    // 12, SDK level 31, Android S.
    return;
  }
  ASSERT_TRUE(font_unique_name_lookup_->UpdateTable());
  blink::FontTableMatcher matcher(
      font_unique_name_lookup_->DuplicateMemoryRegion().Map());
  ASSERT_GT(matcher.AvailableFonts(), 0u);
  auto match_result = matcher.MatchName(kRobotoCondensedBoldItalicNames[1]);
  ASSERT_TRUE(match_result);
  ASSERT_TRUE(EndsWith(match_result->font_path,
                       kRobotoCondensedBoldItalicNames[2],
                       base::CompareCase::SENSITIVE));
  base::File found_file(base::FilePath(match_result->font_path),
                        base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(found_file.IsValid());
  ASSERT_EQ(match_result->ttc_index, 0u);
}

TEST_F(FontUniqueNameLookupTest, TestMatchPostScriptNameTtc) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SdkVersion::SDK_VERSION_NOUGAT) {
    // Pre-Nougat Android does not contain any .ttc files as system fonts.
    return;
  }
  ASSERT_TRUE(font_unique_name_lookup_->UpdateTable());
  blink::FontTableMatcher matcher(
      font_unique_name_lookup_->DuplicateMemoryRegion().Map());
  std::vector<std::string> ttc_postscript_names = {
      "NotoSansCJKjp-Regular",     "NotoSansCJKkr-Regular",
      "NotoSansCJKsc-Regular",     "NotoSansCJKtc-Regular",
      "NotoSansMonoCJKjp-Regular", "NotoSansMonoCJKkr-Regular",
      "NotoSansMonoCJKsc-Regular", "NotoSansMonoCJKtc-Regular",
  };
  // In Android 11 the font file contains addition HK variants as part of the
  // TrueType collection.
  if (base::android::BuildInfo::GetInstance()->sdk_int() >=
      base::android::SdkVersion::SDK_VERSION_R) {
    ttc_postscript_names = std::vector<std::string>(
        {"NotoSansCJKjp-Regular", "NotoSansCJKkr-Regular",
         "NotoSansCJKsc-Regular", "NotoSansCJKtc-Regular",
         "NotoSansCJKhk-Regular", "NotoSansMonoCJKjp-Regular",
         "NotoSansMonoCJKkr-Regular", "NotoSansMonoCJKsc-Regular",
         "NotoSansMonoCJKtc-Regular", "NotoSansMonoCJKhk-Regular"});
  }
  for (size_t i = 0; i < ttc_postscript_names.size(); ++i) {
    auto match_result = matcher.MatchName(ttc_postscript_names[i]);
    ASSERT_TRUE(match_result);
    ASSERT_TRUE(EndsWith(match_result->font_path, "NotoSansCJK-Regular.ttc",
                         base::CompareCase::SENSITIVE));
    base::File found_file(base::FilePath(match_result->font_path),
                          base::File::FLAG_OPEN | base::File::FLAG_READ);
    ASSERT_TRUE(found_file.IsValid());
    ASSERT_EQ(match_result->ttc_index, i);
  }
}

TEST_F(FontUniqueNameLookupTest, TestMatchFullFontName) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() >=
      base::android::SdkVersion::SDK_VERSION_S) {
    // TODO(crbug.com/40203471): Fonts identified by
    // kRobotoCondensedBoldItalicNames do not seem to be available on Android
    // 12, SDK level 31, Android S.
    return;
  }
  ASSERT_TRUE(font_unique_name_lookup_->UpdateTable());
  blink::FontTableMatcher matcher(
      font_unique_name_lookup_->DuplicateMemoryRegion().Map());
  auto match_result = matcher.MatchName(kRobotoCondensedBoldItalicNames[0]);
  ASSERT_TRUE(match_result);
  ASSERT_TRUE(EndsWith(match_result->font_path,
                       kRobotoCondensedBoldItalicNames[2],
                       base::CompareCase::SENSITIVE));
  base::File found_file(base::FilePath(match_result->font_path),
                        base::File::FLAG_OPEN | base::File::Flags::FLAG_READ);
  ASSERT_TRUE(found_file.IsValid());
  ASSERT_EQ(match_result->ttc_index, 0u);
}

TEST_F(FontUniqueNameLookupTest, DontMatchOtherNames) {
  ASSERT_TRUE(font_unique_name_lookup_->UpdateTable());
  blink::FontTableMatcher matcher(
      font_unique_name_lookup_->DuplicateMemoryRegion().Map());
  // Name id 9 is the designer field, which we must not match against.
  auto match_result = matcher.MatchName("Christian Robertson");
  ASSERT_FALSE(match_result);
  // Name id 13 contains the license, which we also must not match.
  match_result =
      matcher.MatchName("Licensed under the Apache License, Version 2.0");
  ASSERT_FALSE(match_result);
}

namespace {
int64_t GetOffsetFirstTable(base::File& font_file) {
  std::vector<uint8_t> bytes;
  bytes.resize(font_file.GetLength());
  CHECK(font_file.ReadAndCheck(0, bytes));
  rust::Slice<const uint8_t> bytes_slice(bytes.data(), bytes.size());
  return name_table_access::offset_first_table(bytes_slice);
}

}  // namespace

// Creates a temp directory and copies Android font files to this
// directory. Provides two methods to inject faults into the font files 1)
// ZeroOutHeader removes the font's or font collection's header
// 2) ZeroAfterHeader writes 0 from after
// the table records until the end of the file.
class FontFileCorruptor {
 public:
  FontFileCorruptor() {
    CHECK(temp_dir_.CreateUniqueTempDir());
    CopyPlatformFilesToTempDir();
  }

  // Overwrite the list of table records with 0.
  void ZeroHeader() {
    ForEachCopiedFontFile([](base::File& font_file) {
      const size_t offset_first_table = GetOffsetFirstTable(font_file);

      std::vector<uint8_t> zeroes;
      zeroes.resize(offset_first_table, 0);
      CHECK(font_file.WriteAndCheck(0, zeroes));
    });
  }

  // Overwrite the data in the font file with zeroes from after the table
  // records until the end of the file.
  void ZeroAfterHeader() {
    ForEachCopiedFontFile([](base::File& font_file) {
      const size_t offset_first_table = GetOffsetFirstTable(font_file);
      std::vector<uint8_t> zeroes;
      zeroes.resize(font_file.GetLength() - offset_first_table, 0);
      CHECK(font_file.WriteAndCheck(offset_first_table, zeroes));
    });
  }

  // Get the list of filenames copied to the temporary directory.
  std::vector<base::FilePath> GetFontFilesList() { return copied_files_; }

 private:
  void ForEachCopiedFontFile(
      base::FunctionRef<void(base::File&)> manipulate_file) {
    for (const auto& filename : copied_files_) {
      base::File font_file(base::FilePath(filename),
                           base::File::FLAG_OPEN | base::File::FLAG_READ |
                               base::File::FLAG_WRITE);
      manipulate_file(font_file);
    }
  }

  void CopyPlatformFilesToTempDir() {
    std::vector<base::FilePath> platform_files = AndroidFontFilesList();
    for (auto& font_file : platform_files) {
      base::FilePath source_path(font_file);
      base::FilePath destination_path(temp_dir_.GetPath());
      destination_path = destination_path.Append(source_path.BaseName());
      if (base::CopyFile(source_path, destination_path)) {
        copied_files_.push_back(destination_path);
      }
    }
  }
  base::ScopedTempDir temp_dir_;
  std::vector<base::FilePath> copied_files_;
};

class FaultInjectingFontUniqueNameLookupTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    font_unique_name_lookup_ =
        std::make_unique<FontUniqueNameLookup>(temp_dir_.GetPath());
    font_unique_name_lookup_->SetFontFilePathsForTesting(
        font_file_corruptor_.GetFontFilesList());
  }

  base::ScopedTempDir temp_dir_;
  FontFileCorruptor font_file_corruptor_;
  std::unique_ptr<FontUniqueNameLookup> font_unique_name_lookup_;
};

TEST_F(FaultInjectingFontUniqueNameLookupTest, TestZeroedTableContents) {
  font_file_corruptor_.ZeroAfterHeader();
  ASSERT_TRUE(font_unique_name_lookup_->UpdateTable());
  blink::FontTableMatcher matcher_after_update(
      font_unique_name_lookup_->DuplicateMemoryRegion().Map());
  ASSERT_EQ(matcher_after_update.AvailableFonts(), 0u);
}

TEST_F(FaultInjectingFontUniqueNameLookupTest, TestZeroedHeader) {
  font_file_corruptor_.ZeroHeader();
  ASSERT_TRUE(font_unique_name_lookup_->UpdateTable());
  blink::FontTableMatcher matcher_after_update(
      font_unique_name_lookup_->DuplicateMemoryRegion().Map());
  ASSERT_EQ(matcher_after_update.AvailableFonts(), 0u);
}

class FontUniqueNameLookupUpdateTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(lookup_table_storage_dir.CreateUniqueTempDir());
    font_unique_name_lookup_ = std::make_unique<FontUniqueNameLookup>(
        lookup_table_storage_dir.GetPath());
    font_unique_name_lookup_->SetFontFilePathsForTesting(
        SplitFontFilesList(AndroidFontFilesList(), false));
    font_unique_name_lookup_->SetAndroidBuildFingerprintForTesting("A");
  }

  base::ScopedTempDir lookup_table_storage_dir;
  std::unique_ptr<FontUniqueNameLookup> font_unique_name_lookup_;
};

TEST_F(FontUniqueNameLookupUpdateTest, CompareSets) {
  ASSERT_TRUE(font_unique_name_lookup_->UpdateTable());
  blink::FontTableMatcher matcher_initial(
      font_unique_name_lookup_->DuplicateMemoryRegion().Map());
  ASSERT_GT(matcher_initial.AvailableFonts(), 0u);
  font_unique_name_lookup_->SetFontFilePathsForTesting(
      SplitFontFilesList(AndroidFontFilesList(), true));
  // Set the Android build fingerprint to something different from what it's set
  // to in the test's SetUp method to trigger re-indexing.
  font_unique_name_lookup_->SetAndroidBuildFingerprintForTesting("B");
  font_unique_name_lookup_->UpdateTableIfNeeded();
  blink::FontTableMatcher matcher_second_half(
      font_unique_name_lookup_->DuplicateMemoryRegion().Map());
  ASSERT_GT(matcher_initial.AvailableFonts(), 0u);
  ASSERT_TRUE(matcher_initial.FontListIsDisjointFrom(matcher_second_half));
}

}  // namespace content
