// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/android/build_info.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "content/browser/font_unique_name_lookup/font_unique_name_lookup.h"
#include "third_party/blink/public/common/font_unique_name_lookup/font_table_matcher.h"

#include <functional>
#include <memory>

namespace {

static const char* const kAndroidFontPaths[] = {"/system/fonts",
                                                "/vendor/fonts"};

// Full font name, postscript name, filename.
static const char* const kRobotoCondensedBoldItalicNames[] = {
    "Roboto Condensed Bold Italic", "RobotoCondensed-BoldItalic",
    "RobotoCondensed-BoldItalic.ttf"};

std::vector<std::string> AndroidFontFilesList() {
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

std::vector<std::string> SplitFontFilesList(
    const std::vector<std::string> font_files,
    bool return_second_half) {
  CHECK_GT(font_files.size(), 2u);
  auto start_copy = font_files.begin();
  auto end_copy = font_files.begin() + (font_files.size() / 2);
  if (return_second_half) {
    start_copy = end_copy;
    end_copy = font_files.end();
  }
  return std::vector<std::string>(start_copy, end_copy);
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
  // Marshmallow has 149, Oreo 247, let's expect at least 50.
  ASSERT_GT(matcher.AvailableFonts(), 50u);
  ASSERT_TRUE(font_unique_name_lookup_->PersistToFile());
  ASSERT_TRUE(base::PathExists(
      font_unique_name_lookup_->TableCacheFilePathForTesting()));
  int64_t file_size;
  ASSERT_TRUE(base::GetFileSize(
      font_unique_name_lookup_->TableCacheFilePathForTesting(), &file_size));
  // For 81 fonts minimumm, very conservatively assume we have at least 1k of
  // data, it's rather around 30k in practice.
  ASSERT_GT(file_size, 1024);
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
size_t GetNumTables(base::File& font_file) {
  font_file.Seek(base::File::FROM_BEGIN, 5);
  uint8_t num_tables_bytes[2] = {};
  font_file.ReadAtCurrentPos(reinterpret_cast<char*>(num_tables_bytes),
                             base::size(num_tables_bytes));
  uint16_t num_tables =
      static_cast<uint16_t>(num_tables_bytes[0] + (num_tables_bytes[1] << 8));
  return num_tables;
}

const size_t kOffsetTableRecords = 13;
const size_t kSizeOneTableRecord = 16;

}  // namespace

// Creates a temp directory and copies Android font files to this
// directory. Provides two methods to inject faults into the font files 1)
// ZeroOutTableRecords writes a sequence of 0 to where the font table offset
// should be stored in the font file. 2) ZeroAfterTableIndex writes 0 from after
// the table records until the end of the file.
class FontFileCorruptor {
 public:
  FontFileCorruptor() {
    CHECK(temp_dir_.CreateUniqueTempDir());
    CopyPlatformFilesToTempDir();
  }

  // Overwrite the list of table records with 0.
  void ZeroOutTableRecords() {
    ForEachCopiedFontFile([](base::File& font_file) {
      // Read number of font tables, then zero out the table record structure.
      // https://docs.microsoft.com/en-us/typography/opentype/spec/font-file
      size_t num_tables = GetNumTables(font_file);
      CHECK_GT(num_tables, 0u);
      char garbage[kSizeOneTableRecord] = {0};
      for (size_t i = 0; i < num_tables; ++i) {
        CHECK_EQ(static_cast<int>(kSizeOneTableRecord),
                 font_file.Write(kOffsetTableRecords + i * kSizeOneTableRecord,
                                 garbage, base::size(garbage)));
      }
    });
  }

  // Overwrite the data in the font file with zeroes from after the table
  // records until the end of the file.
  void ZeroAfterTableIndex() {
    ForEachCopiedFontFile([](base::File& font_file) {
      size_t num_tables = GetNumTables(font_file);
      CHECK_GT(num_tables, 0u);
      const size_t offset_after_table_records =
          kOffsetTableRecords + num_tables * kSizeOneTableRecord;
      std::vector<char> zeroes;
      zeroes.resize(font_file.GetLength() - offset_after_table_records);
      std::fill(zeroes.begin(), zeroes.end(), 0);
      CHECK_EQ(static_cast<int>(zeroes.size()),
               font_file.Write(offset_after_table_records, zeroes.data(),
                               zeroes.size()));
    });
  }

  // Get the list of filenames copied to the temporary directory.
  std::vector<std::string> GetFontFilesList() { return copied_files_; }

 private:
  void ForEachCopiedFontFile(std::function<void(base::File&)> manipulate_file) {
    for (const auto& filename : copied_files_) {
      base::File font_file(base::FilePath(filename),
                           base::File::FLAG_OPEN | base::File::FLAG_READ |
                               base::File::FLAG_WRITE);
      manipulate_file(font_file);
    }
  }

  void CopyPlatformFilesToTempDir() {
    std::vector<std::string> platform_files = AndroidFontFilesList();
    for (auto& font_file : platform_files) {
      base::FilePath source_path(font_file);
      base::FilePath destination_path(temp_dir_.GetPath());
      destination_path = destination_path.Append(source_path.BaseName());
      if (base::CopyFile(source_path, destination_path))
        copied_files_.push_back(destination_path.value());
    }
  }
  base::ScopedTempDir temp_dir_;
  std::vector<std::string> copied_files_;
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
  font_file_corruptor_.ZeroAfterTableIndex();
  ASSERT_TRUE(font_unique_name_lookup_->UpdateTable());
  blink::FontTableMatcher matcher_after_update(
      font_unique_name_lookup_->DuplicateMemoryRegion().Map());
  ASSERT_EQ(matcher_after_update.AvailableFonts(), 0u);
}

TEST_F(FaultInjectingFontUniqueNameLookupTest, TestZeroedTableIndex) {
  font_file_corruptor_.ZeroOutTableRecords();
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
