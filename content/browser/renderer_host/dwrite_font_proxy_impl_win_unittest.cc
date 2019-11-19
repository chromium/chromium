// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/dwrite_font_proxy_impl_win.h"

#include <dwrite.h>
#include <dwrite_2.h>

#include <memory>

#include "base/file_version_info.h"
#include "base/files/file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/win/windows_version.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/dwrite_rasterizer_support/dwrite_rasterizer_support.h"
#include "third_party/blink/public/common/font_unique_name_lookup/font_table_matcher.h"
#include "third_party/icu/source/common/unicode/umachine.h"
#include "ui/gfx/test/font_fallback_test_data.h"

namespace content {

namespace {

struct FontExpectation {
  const char font_name[64];
  uint16_t ttc_index;
};

constexpr FontExpectation kExpectedTestFonts[] = {{u8"CambriaMath", 1},
                                                  {u8"Ming-Lt-HKSCS-ExtB", 2},
                                                  {u8"NSimSun", 1},
                                                  {u8"calibri-bolditalic", 0}};

// DirectWrite on Windows supports IDWriteFontSet API which allows for querying
// by PostScript name and full font name directly. In the implementation of
// DWriteFontProxy we check whether this API is available by checking for
// whether IDWriteFactory3 is available. In order to validate in a unit test
// whether this check works, compare it against the dwrite.dll major version -
// versions starting from 10 have the required functionality.
constexpr int kDWriteMajorVersionSupportingSingleLookups = 10;

// Base test class that sets up the Mojo connection to DWriteFontProxy so that
// tests can call its Mojo methods.
class DWriteFontProxyImplUnitTest : public testing::Test {
 public:
  DWriteFontProxyImplUnitTest()
      : receiver_(&impl_, dwrite_font_proxy_.BindNewPipeAndPassReceiver()) {}

  blink::mojom::DWriteFontProxy& dwrite_font_proxy() {
    return *dwrite_font_proxy_;
  }

  bool SupportsSingleLookups() {
    blink::mojom::UniqueFontLookupMode lookup_mode;
    dwrite_font_proxy().GetUniqueFontLookupMode(&lookup_mode);
    return lookup_mode == blink::mojom::UniqueFontLookupMode::kSingleLookups;
  }

  base::test::TaskEnvironment task_environment_;
  mojo::Remote<blink::mojom::DWriteFontProxy> dwrite_font_proxy_;
  DWriteFontProxyImpl impl_;
  mojo::Receiver<blink::mojom::DWriteFontProxy> receiver_;
};

// Derived class for tests that exercise font unique local matching mojo methods
// of DWriteFontProxy. Needs a ScopedFeatureList to activate the feature as it
// is currently behind a flag.
class DWriteFontProxyLocalMatchingTest : public DWriteFontProxyImplUnitTest {
 public:
  DWriteFontProxyLocalMatchingTest() {
    feature_list_.InitAndEnableFeature(features::kFontSrcLocalMatching);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Derived class for tests that exercise the parts of the DWriteFontProxy Mojo
// interface that deal with accessing the font lookup table created by
// DWriteFontLookupTableBuilder. Initializes the DWriteFontLookupTableBuilder
// and has a ScopedTempDir for testing persisting the lookup table to disk.
class DWriteFontProxyTableMatchingTest
    : public DWriteFontProxyLocalMatchingTest {
 public:
  void SetUp() override {
    DWriteFontLookupTableBuilder* table_builder_instance =
        DWriteFontLookupTableBuilder::GetInstance();
    bool temp_dir_success = scoped_temp_dir_.CreateUniqueTempDir();
    ASSERT_TRUE(temp_dir_success);
    table_builder_instance->OverrideDWriteVersionChecksForTesting();
    table_builder_instance->SetCacheDirectoryForTesting(
        scoped_temp_dir_.GetPath());
    table_builder_instance->ResetLookupTableForTesting();
    table_builder_instance->SchedulePrepareFontUniqueNameTableIfNeeded();
  }

  void TearDown() override {
    DWriteFontLookupTableBuilder* table_builder_instance =
        DWriteFontLookupTableBuilder::GetInstance();
    table_builder_instance->ResetStateForTesting();
  }

 private:
  base::ScopedTempDir scoped_temp_dir_;
};

TEST_F(DWriteFontProxyImplUnitTest, GetFamilyCount) {
  UINT32 family_count = 0;
  dwrite_font_proxy().GetFamilyCount(&family_count);
  EXPECT_NE(0u, family_count);  // Assume there's some fonts on the test system.
}

TEST_F(DWriteFontProxyImplUnitTest, FindFamily) {
  UINT32 arial_index = 0;
  dwrite_font_proxy().FindFamily(L"Arial", &arial_index);
  EXPECT_NE(UINT_MAX, arial_index);

  UINT32 times_index = 0;
  dwrite_font_proxy().FindFamily(L"Times New Roman", &times_index);
  EXPECT_NE(UINT_MAX, times_index);
  EXPECT_NE(arial_index, times_index);

  UINT32 unknown_index = 0;
  dwrite_font_proxy().FindFamily(L"Not a font family", &unknown_index);
  EXPECT_EQ(UINT_MAX, unknown_index);
}

TEST_F(DWriteFontProxyImplUnitTest, GetFamilyNames) {
  UINT32 arial_index = 0;
  dwrite_font_proxy().FindFamily(L"Arial", &arial_index);

  std::vector<blink::mojom::DWriteStringPairPtr> names;
  dwrite_font_proxy().GetFamilyNames(arial_index, &names);

  EXPECT_LT(0u, names.size());
  for (const auto& pair : names) {
    EXPECT_FALSE(pair->first.empty());
    EXPECT_FALSE(pair->second.empty());
  }
}

TEST_F(DWriteFontProxyImplUnitTest, GetFamilyNamesIndexOutOfBounds) {
  std::vector<blink::mojom::DWriteStringPairPtr> names;
  UINT32 invalid_index = 1000000;
  dwrite_font_proxy().GetFamilyNames(invalid_index, &names);

  EXPECT_TRUE(names.empty());
}

TEST_F(DWriteFontProxyImplUnitTest, GetFontFiles) {
  UINT32 arial_index = 0;
  dwrite_font_proxy().FindFamily(L"Arial", &arial_index);

  std::vector<base::FilePath> files;
  std::vector<base::File> handles;
  dwrite_font_proxy().GetFontFiles(arial_index, &files, &handles);

  EXPECT_LT(0u, files.size());
  for (const auto& file : files) {
    EXPECT_FALSE(file.value().empty());
  }
}

TEST_F(DWriteFontProxyImplUnitTest, GetFontFilesIndexOutOfBounds) {
  std::vector<base::FilePath> files;
  std::vector<base::File> handles;
  UINT32 invalid_index = 1000000;
  dwrite_font_proxy().GetFontFiles(invalid_index, &files, &handles);

  EXPECT_EQ(0u, files.size());
}

TEST_F(DWriteFontProxyImplUnitTest, MapCharacter) {
  if (!blink::DWriteRasterizerSupport::IsDWriteFactory2Available())
    return;

  blink::mojom::MapCharactersResultPtr result;
  dwrite_font_proxy().MapCharacters(
      L"abc",
      blink::mojom::DWriteFontStyle::New(DWRITE_FONT_WEIGHT_NORMAL,
                                         DWRITE_FONT_STYLE_NORMAL,
                                         DWRITE_FONT_STRETCH_NORMAL),
      L"", DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, L"", &result);

  EXPECT_NE(UINT32_MAX, result->family_index);
  EXPECT_STRNE(L"", result->family_name.c_str());
  EXPECT_EQ(3u, result->mapped_length);
  EXPECT_NE(0.0, result->scale);
  EXPECT_NE(0, result->font_style->font_weight);
  EXPECT_EQ(DWRITE_FONT_STYLE_NORMAL, result->font_style->font_slant);
  EXPECT_NE(0, result->font_style->font_stretch);
}

TEST_F(DWriteFontProxyImplUnitTest, MapCharacterInvalidCharacter) {
  if (!blink::DWriteRasterizerSupport::IsDWriteFactory2Available())
    return;

  blink::mojom::MapCharactersResultPtr result;
  dwrite_font_proxy().MapCharacters(
      L"\ufffe\uffffabc",
      blink::mojom::DWriteFontStyle::New(DWRITE_FONT_WEIGHT_NORMAL,
                                         DWRITE_FONT_STYLE_NORMAL,
                                         DWRITE_FONT_STRETCH_NORMAL),
      L"en-us", DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, L"", &result);

  EXPECT_EQ(UINT32_MAX, result->family_index);
  EXPECT_STREQ(L"", result->family_name.c_str());
  EXPECT_EQ(2u, result->mapped_length);
}

TEST_F(DWriteFontProxyImplUnitTest, MapCharacterInvalidAfterValid) {
  if (!blink::DWriteRasterizerSupport::IsDWriteFactory2Available())
    return;

  blink::mojom::MapCharactersResultPtr result;
  dwrite_font_proxy().MapCharacters(
      L"abc\ufffe\uffff",
      blink::mojom::DWriteFontStyle::New(DWRITE_FONT_WEIGHT_NORMAL,
                                         DWRITE_FONT_STYLE_NORMAL,
                                         DWRITE_FONT_STRETCH_NORMAL),
      L"en-us", DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, L"", &result);

  EXPECT_NE(UINT32_MAX, result->family_index);
  EXPECT_STRNE(L"", result->family_name.c_str());
  EXPECT_EQ(3u, result->mapped_length);
  EXPECT_NE(0.0, result->scale);
  EXPECT_NE(0, result->font_style->font_weight);
  EXPECT_EQ(DWRITE_FONT_STYLE_NORMAL, result->font_style->font_slant);
  EXPECT_NE(0, result->font_style->font_stretch);
}

TEST_F(DWriteFontProxyImplUnitTest, TestCustomFontFiles) {
  // Override windows fonts path to force the custom font file codepath.
  impl_.SetWindowsFontsPathForTesting(L"X:\\NotWindowsFonts");

  UINT32 arial_index = 0;
  dwrite_font_proxy().FindFamily(L"Arial", &arial_index);

  std::vector<base::FilePath> files;
  std::vector<base::File> handles;
  dwrite_font_proxy().GetFontFiles(arial_index, &files, &handles);

  EXPECT_TRUE(files.empty());
  EXPECT_FALSE(handles.empty());
  for (auto& file : handles) {
    EXPECT_TRUE(file.IsValid());
    EXPECT_LT(0, file.GetLength());  // Check the file exists
  }
}

TEST_F(DWriteFontProxyImplUnitTest, FallbackFamily) {
  const bool on_win10 = base::win::GetVersion() >= base::win::Version::WIN10;

  for (auto& fallback_request : gfx::kGetFontFallbackTests) {
    if (fallback_request.is_win10 && !on_win10)
      continue;

    blink::mojom::FallbackFamilyAndStylePtr fallback_family_and_style;
    UChar32 codepoint;
    U16_GET(fallback_request.text.c_str(), 0, 0, fallback_request.text.size(),
            codepoint);
    dwrite_font_proxy().FallbackFamilyAndStyleForCodepoint(
        "Times New Roman", fallback_request.language_tag, codepoint,
        &fallback_family_and_style);

    auto find_result_it =
        std::find(fallback_request.fallback_fonts.begin(),
                  fallback_request.fallback_fonts.end(),
                  fallback_family_and_style->fallback_family_name);

    EXPECT_TRUE(find_result_it != fallback_request.fallback_fonts.end())
        << "Did not find expected fallback font for language: "
        << fallback_request.language_tag << ", codepoint U+" << std::hex
        << codepoint << " DWrite returned font name: \""
        << fallback_family_and_style->fallback_family_name << "\""
        << ", expected: "
        << base::JoinString(fallback_request.fallback_fonts, ", ");
    EXPECT_EQ(fallback_family_and_style->weight, 400u);
    EXPECT_EQ(fallback_family_and_style->width,
              5u);  // SkFontStyle::Width::kNormal_Width
    EXPECT_EQ(fallback_family_and_style->slant,
              0u);  // SkFontStyle::Slant::kUpright_Slant
  }
}

namespace {
void TestWhenLookupTableReady(
    bool* did_test_fonts,
    base::ReadOnlySharedMemoryRegion font_table_memory) {
  blink::FontTableMatcher font_table_matcher(font_table_memory.Map());
  for (auto& test_font_name_index : kExpectedTestFonts) {
    base::Optional<blink::FontTableMatcher::MatchResult> match_result =
        font_table_matcher.MatchName(test_font_name_index.font_name);
    ASSERT_TRUE(match_result)
        << "No font matched for font name: " << test_font_name_index.font_name;
    base::File unique_font_file(
        base::FilePath::FromUTF8Unsafe(match_result->font_path),
        base::File::FLAG_OPEN | base::File::FLAG_READ);
    ASSERT_TRUE(unique_font_file.IsValid());
    ASSERT_GT(unique_font_file.GetLength(), 0);
    ASSERT_EQ(test_font_name_index.ttc_index, match_result->ttc_index);
    *did_test_fonts = true;
  }
}
}  // namespace

TEST_F(DWriteFontProxyTableMatchingTest, TestFindUniqueFont) {
  bool lookup_table_results_were_tested = false;
  dwrite_font_proxy().GetUniqueNameLookupTable(base::BindOnce(
      &TestWhenLookupTableReady, &lookup_table_results_were_tested));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(lookup_table_results_were_tested);
}

TEST_F(DWriteFontProxyLocalMatchingTest, TestSingleLookup) {
  // Do not run this test on unsupported Windows versions.
  if (!SupportsSingleLookups())
    return;
  for (auto& test_font_name_index : kExpectedTestFonts) {
    base::FilePath result_path;
    uint32_t ttc_index;
    dwrite_font_proxy().MatchUniqueFont(
        base::UTF8ToUTF16(test_font_name_index.font_name), &result_path,
        &ttc_index);
    ASSERT_GT(result_path.value().size(), 0u);
    base::File unique_font_file(result_path,
                                base::File::FLAG_OPEN | base::File::FLAG_READ);
    ASSERT_TRUE(unique_font_file.IsValid());
    ASSERT_GT(unique_font_file.GetLength(), 0);
    ASSERT_EQ(test_font_name_index.ttc_index, ttc_index);
  }
}

TEST_F(DWriteFontProxyLocalMatchingTest, TestSingleLookupUnavailable) {
  // Do not run this test on unsupported Windows versions.
  if (!SupportsSingleLookups())
    return;
  base::FilePath result_path;
  uint32_t ttc_index;
  std::string unavailable_font_name =
      "Unavailable_Font_Name_56E7EA7E-2C69-4E23-99DC-750BC19B250E";
  dwrite_font_proxy().MatchUniqueFont(base::UTF8ToUTF16(unavailable_font_name),
                                      &result_path, &ttc_index);
  ASSERT_EQ(result_path.value().size(), 0u);
  ASSERT_EQ(ttc_index, 0u);
}

TEST_F(DWriteFontProxyLocalMatchingTest, TestLookupMode) {
  std::unique_ptr<FileVersionInfo> dwrite_version_info =
      FileVersionInfo::CreateFileVersionInfo(
          base::FilePath(FILE_PATH_LITERAL("DWrite.dll")));

  std::string dwrite_version =
      base::WideToUTF8(dwrite_version_info->product_version());

  int dwrite_major_version_number =
      std::stoi(dwrite_version.substr(0, dwrite_version.find(".")));

  blink::mojom::UniqueFontLookupMode expected_lookup_mode;
  if (dwrite_major_version_number >=
      kDWriteMajorVersionSupportingSingleLookups) {
    expected_lookup_mode = blink::mojom::UniqueFontLookupMode::kSingleLookups;
  } else {
    expected_lookup_mode = blink::mojom::UniqueFontLookupMode::kRetrieveTable;
  }

  blink::mojom::UniqueFontLookupMode lookup_mode;
  dwrite_font_proxy().GetUniqueFontLookupMode(&lookup_mode);
  ASSERT_EQ(lookup_mode, expected_lookup_mode);
}

}  // namespace

}  // namespace content
