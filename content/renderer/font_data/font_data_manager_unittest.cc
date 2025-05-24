// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/font_data/font_data_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/cstring_view.h"
#include "base/test/task_environment.h"
#include "components/services/font_data/public/mojom/font_data_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "skia/ext/font_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkFourByteTag.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace content {

namespace {

// Wrapper that implements FontDataService interface for unit tests purposes.
// Replaces the mojo receiver in FontDataManager to track call counts and
// simulate responses.
class TestFontServiceApp : public font_data_service::mojom::FontDataService {
 public:
  TestFontServiceApp() = default;
  TestFontServiceApp(const TestFontServiceApp&) = delete;
  TestFontServiceApp& operator=(const TestFontServiceApp&) = delete;

  mojo::PendingRemote<font_data_service::mojom::FontDataService>
  CreateRemote() {
    mojo::PendingRemote<font_data_service::mojom::FontDataService> proxy;
    receivers_.Add(this, proxy.InitWithNewPipeAndPassReceiver());
    return proxy;
  }

  void MatchFamilyName(const std::string& family_name,
                       font_data_service::mojom::TypefaceStylePtr style,
                       MatchFamilyNameCallback callback) override {
    match_family_call_count_++;
    int ttc_index = 0;
    SkFontStyle font_style(style->weight, style->width,
                           static_cast<SkFontStyle::Slant>(style->slant));
    sk_sp<SkTypeface> typeface =
        skia::MakeTypefaceFromName(family_name.c_str(), font_style);
    std::unique_ptr<SkStreamAsset> asset = typeface->openStream(&ttc_index);
    auto result = font_data_service::mojom::MatchFamilyNameResult::New();
    result->ttc_index = ttc_index;
    const int axis_count = typeface->getVariationDesignPosition(nullptr, 0);
    if (axis_count > 0) {
      std::vector<SkFontArguments::VariationPosition::Coordinate>
          coordinate_list;
      coordinate_list.resize(axis_count);
      if (typeface->getVariationDesignPosition(coordinate_list.data(),
                                               coordinate_list.size()) > 0) {
        auto variation_position =
            font_data_service::mojom::VariationPosition::New();
        for (const auto& coordinate : coordinate_list) {
          auto coordinate_result = font_data_service::mojom::Coordinate::New();
          coordinate_result->axis = coordinate.axis;
          coordinate_result->value = coordinate.value;
          variation_position->coordinates.push_back(
              std::move(coordinate_result));
        }
        variation_position->coordinateCount = axis_count;
        result->variation_position = std::move(variation_position);
      }
    }

    if (use_memory_fallback_) {
      memory_map_region_ =
          base::ReadOnlySharedMemoryRegion::Create(asset->getLength());
      EXPECT_TRUE(memory_map_region_.IsValid());

      size_t bytes_read = asset->read(memory_map_region_.mapping.memory(),
                                      memory_map_region_.mapping.size());
      EXPECT_EQ(bytes_read, asset->getLength());
      font_data_service::mojom::TypefaceDataPtr typeface_data =
          font_data_service::mojom::TypefaceData::NewRegion(
              memory_map_region_.region.Duplicate());
      EXPECT_TRUE(typeface_data->get_region().IsValid());
      result->typeface_data = std::move(typeface_data);
    } else {
      SkString font_path;
      typeface->getResourceName(&font_path);
      base::File font_file =
          base::File(base::FilePath::FromUTF8Unsafe(font_path.c_str()),
                     base::File::FLAG_OPEN | base::File::FLAG_READ |
                         base::File::FLAG_WIN_EXCLUSIVE_WRITE);
      result->typeface_data =
          font_data_service::mojom::TypefaceData::NewFontFile(
              std::move(font_file));
    }
    std::move(callback).Run(std::move(result));
  }

  void MatchFamilyNameCharacter(
      const std::string& family_name,
      font_data_service::mojom::TypefaceStylePtr style,
      const std::vector<std::string>& bcp47s,
      int32_t character,
      MatchFamilyNameCharacterCallback callback) override {
    ++match_family_character_call_count_;
    last_match_family_character_call_bcp47s_ = bcp47s;
    last_match_family_character_call_character_ = character;
    MatchFamilyName(family_name, std::move(style), std::move(callback));
  }

  void GetAllFamilyNames(GetAllFamilyNamesCallback callback) override {
    std::vector<std::string> names{"First Font", "Other Font"};
    std::move(callback).Run(std::move(names));
  }

  void LegacyMakeTypeface(const std::optional<std::string>& family_name,
                          font_data_service::mojom::TypefaceStylePtr style,
                          LegacyMakeTypefaceCallback callback) override {
    // This isn't usually implemented in terms of MatchFamilyName, but this
    // mock's MatchFamilyName always returns a typeface so it's fine to do this
    // in this case. The important bit is that `LegacyMakeTypeface` return a
    // font when passed a `null` font family name, which is the default font in
    // real code.
    ++legacy_make_typeface_call_count_;
    MatchFamilyName(family_name ? *family_name : "", std::move(style),
                    std::move(callback));
  }

  size_t match_family_call_count() const { return match_family_call_count_; }
  size_t match_family_character_call_count() const {
    return match_family_character_call_count_;
  }
  const std::vector<std::string>& last_match_family_character_call_bcp47s()
      const {
    return last_match_family_character_call_bcp47s_;
  }
  int32_t last_match_family_character_call_character() const {
    return last_match_family_character_call_character_;
  }
  size_t legacy_make_typeface_call_count() const {
    return legacy_make_typeface_call_count_;
  }
  void set_use_memory_fallback(bool fallback) {
    use_memory_fallback_ = fallback;
  }

 private:
  mojo::ReceiverSet<font_data_service::mojom::FontDataService> receivers_;
  size_t match_family_call_count_ = 0;
  size_t match_family_character_call_count_ = 0;
  std::vector<std::string> last_match_family_character_call_bcp47s_;
  int32_t last_match_family_character_call_character_;
  size_t legacy_make_typeface_call_count_ = 0;
  base::MappedReadOnlyRegion memory_map_region_;
  bool use_memory_fallback_ = false;
};

// Wrapper class used to verify SkFontArguments in FontDataManager.
class TestFontDataManager : public font_data_service::FontDataManager {
 public:
  TestFontDataManager(int expected_coordinate_count,
                      int expected_ttc_index,
                      SkFourByteTag expected_first_coordinate_axis,
                      int expected_first_coordinate_value)
      : expected_coordinate_count_(expected_coordinate_count),
        expected_ttc_index_(expected_ttc_index),
        expected_first_coordinate_axis_(expected_first_coordinate_axis),
        expected_first_coordinate_value_(expected_first_coordinate_value) {}
  TestFontDataManager() = delete;
  TestFontDataManager(const TestFontServiceApp&) = delete;
  TestFontDataManager& operator=(const TestFontServiceApp&) = delete;

  sk_sp<SkTypeface> onMakeFromStreamArgs(
      std::unique_ptr<SkStreamAsset> asset,
      const SkFontArguments& font_arguments) const override {
    EXPECT_EQ(expected_coordinate_count_,
              font_arguments.getVariationDesignPosition().coordinateCount);
    if (font_arguments.getVariationDesignPosition().coordinateCount > 0) {
      EXPECT_EQ(font_arguments.getVariationDesignPosition().coordinates[0].axis,
                expected_first_coordinate_axis_);
      EXPECT_EQ(
          font_arguments.getVariationDesignPosition().coordinates[0].value,
          expected_first_coordinate_value_);
    }
    EXPECT_EQ(expected_ttc_index_, font_arguments.getCollectionIndex());
    return font_data_service::FontDataManager::onMakeFromStreamArgs(
        std::move(asset), font_arguments);
  }

 private:
  int expected_coordinate_count_;
  int expected_ttc_index_;
  SkFourByteTag expected_first_coordinate_axis_;
  int expected_first_coordinate_value_;
};

class FontDataManagerUnitTest : public testing::Test {
 protected:
  FontDataManagerUnitTest()
      : skia_font_manager_(sk_make_sp<font_data_service::FontDataManager>()) {
    skia_font_manager_->SetFontServiceForTesting(
        test_font_data_service_app_.CreateRemote());
  }
  base::test::SingleThreadTaskEnvironment task_environment_;
  TestFontServiceApp test_font_data_service_app_;
  sk_sp<font_data_service::FontDataManager> skia_font_manager_;
};

TEST_F(FontDataManagerUnitTest, MatchFamilyStyle) {
  SkFontStyle style(400, 5, SkFontStyle::kUpright_Slant);
  base::cstring_view family_name = "Segoe UI";
  sk_sp<SkTypeface> expected_typeface =
      skia::MakeTypefaceFromName(family_name.data(), style);

  // Test the initial typeface matches family name and font style.
  sk_sp<SkTypeface> result =
      skia_font_manager_->matchFamilyStyle(family_name.data(), style);
  EXPECT_EQ(result->fontStyle(), expected_typeface->fontStyle());
  SkString result_family_name;
  result->getFamilyName(&result_family_name);
  EXPECT_STREQ(result_family_name.c_str(), family_name.data());
  EXPECT_EQ(test_font_data_service_app_.match_family_call_count(), 1u);

  // Test that the cache works.
  // Same style.
  result = skia_font_manager_->matchFamilyStyle(family_name.data(), style);
  EXPECT_EQ(test_font_data_service_app_.match_family_call_count(), 1u);

  // Test with a different style.
  SkFontStyle bold_style(600, 5, SkFontStyle::kUpright_Slant);
  result = skia_font_manager_->matchFamilyStyle(family_name.data(), bold_style);
  EXPECT_EQ(result->fontStyle(), bold_style);
  EXPECT_EQ(test_font_data_service_app_.match_family_call_count(), 2u);

  // Test with a different family name and legacy method.
  family_name = "Arial";
  result = skia_font_manager_->legacyMakeTypeface(family_name.data(), style);
  result->getFamilyName(&result_family_name);
  EXPECT_STREQ(result_family_name.c_str(), family_name.data());
  EXPECT_EQ(test_font_data_service_app_.match_family_call_count(), 3u);
}

TEST_F(FontDataManagerUnitTest, LegacyMakeTypefaceNullFamilyName) {
  SkFontStyle style(400, 5, SkFontStyle::kUpright_Slant);

  sk_sp<SkTypeface> result =
      skia_font_manager_->legacyMakeTypeface(nullptr, style);
  SkString result_family_name;
  result->getFamilyName(&result_family_name);
  EXPECT_GT(result_family_name.size(), 0UL);
  EXPECT_EQ(test_font_data_service_app_.legacy_make_typeface_call_count(), 1u);
}

TEST_F(FontDataManagerUnitTest, MatchFamilyStyleWithMemoryRegion) {
  test_font_data_service_app_.set_use_memory_fallback(true);
  SkFontStyle style(400, 5, SkFontStyle::kUpright_Slant);
  base::cstring_view family_name = "Segoe UI";
  sk_sp<SkTypeface> expected_typeface =
      skia::MakeTypefaceFromName(family_name.data(), style);

  // Test the initial typeface matches family name and font style.
  sk_sp<SkTypeface> result =
      skia_font_manager_->matchFamilyStyle(family_name.data(), style);
  EXPECT_EQ(result->fontStyle(), expected_typeface->fontStyle());
  SkString result_family_name;
  result->getFamilyName(&result_family_name);
  EXPECT_STREQ(result_family_name.c_str(), family_name.data());
  EXPECT_EQ(test_font_data_service_app_.match_family_call_count(), 1u);
}

TEST_F(FontDataManagerUnitTest, FontArgumentTest) {
  // Bahnschrift is a font family with 2 axes hence coordinate count should
  // be 2.
  SkFourByteTag axis =
      SkSetFourByteTag('w', 'g', 'h', 't');  // Tag for "Weight"
  int font_weight = 400;
  auto font_manager = sk_make_sp<TestFontDataManager>(2, 0, axis, font_weight);
  font_manager->SetFontServiceForTesting(
      test_font_data_service_app_.CreateRemote());
  base::cstring_view family_name = "Bahnschrift";
  SkString result_family_name;
  sk_sp<SkTypeface> result = font_manager->matchFamilyStyle(
      family_name.data(), {font_weight, 5, SkFontStyle::kUpright_Slant});
  result->getFamilyName(&result_family_name);
  EXPECT_STREQ(result_family_name.c_str(), family_name.data());
  EXPECT_EQ(test_font_data_service_app_.match_family_call_count(), 1u);
}

TEST_F(FontDataManagerUnitTest, MakeFromData) {
  SkFontStyle style(400, 5, SkFontStyle::kUpright_Slant);
  constexpr base::cstring_view family_name = "Segoe UI";
  int ttc_index = 0;
  sk_sp<SkTypeface> typeface =
      skia::MakeTypefaceFromName(family_name.data(), style);
  std::unique_ptr<SkStreamAsset> asset = typeface->openStream(&ttc_index);
  sk_sp<SkTypeface> result = skia_font_manager_->makeFromData(
      SkData::MakeFromStream(asset.get(), asset->getLength()), 0);

  SkString result_family_name;
  result->getFamilyName(&result_family_name);
  EXPECT_STREQ(result_family_name.c_str(), family_name.data());
}

TEST_F(FontDataManagerUnitTest, MatchFamilyStyleCharacter) {
  SkFontStyle style(400, 5, SkFontStyle::kUpright_Slant);
  SkUnichar uni_char = 0x0041;  // 'A'
  base::cstring_view family_name = "Segoe UI";
  sk_sp<SkTypeface> expected_typeface =
      skia::MakeTypefaceFromName(family_name.data(), style);

  // Test the initial typeface matches family name and font style.
  const char kChineseBcp47[] = "zh";
  const char* kBcp47Array[] = {kChineseBcp47};
  sk_sp<SkTypeface> result = skia_font_manager_->matchFamilyStyleCharacter(
      family_name.data(), style, kBcp47Array, 1, uni_char);
  EXPECT_EQ(result->fontStyle(), expected_typeface->fontStyle());
  SkString result_family_name;
  result->getFamilyName(&result_family_name);
  EXPECT_STREQ(result_family_name.c_str(), family_name.data());
  EXPECT_EQ(test_font_data_service_app_.match_family_call_count(), 1u);

  std::vector<std::string> kExpectedBcp47s{"zh"};
  EXPECT_EQ(
      test_font_data_service_app_.last_match_family_character_call_character(),
      uni_char);
  EXPECT_EQ(
      test_font_data_service_app_.last_match_family_character_call_bcp47s(),
      kExpectedBcp47s);
}

TEST_F(FontDataManagerUnitTest, CountFamilies) {
  // The TestFontServiceApp returns a vector of 2 family names.
  EXPECT_EQ(skia_font_manager_->countFamilies(), 2);
}

TEST_F(FontDataManagerUnitTest, GetFamilyName) {
  SkString family_name;
  skia_font_manager_->getFamilyName(0, &family_name);
  EXPECT_EQ(family_name, SkString("First Font"));

  skia_font_manager_->getFamilyName(1, &family_name);
  EXPECT_EQ(family_name, SkString("Other Font"));

  SkString empty_family_name;

  // Asking for a negative index doesn't change the output parameter.
  skia_font_manager_->getFamilyName(-1, &empty_family_name);
  EXPECT_TRUE(empty_family_name.isEmpty());

  // There are only 2 font families, asking for index 2 doesn't change the
  // output parameter.
  skia_font_manager_->getFamilyName(2, &empty_family_name);
  EXPECT_TRUE(empty_family_name.isEmpty());
}

using FontDataManagerDeathTest = FontDataManagerUnitTest;

// Methods are unused in FontDataManager.
TEST_F(FontDataManagerDeathTest, CreateStyleSet) {
  EXPECT_DEATH_IF_SUPPORTED(skia_font_manager_->createStyleSet(0), "");
}

TEST_F(FontDataManagerDeathTest, MatchFamily) {
  EXPECT_DEATH_IF_SUPPORTED(skia_font_manager_->matchFamily("Segoe UI"), "");
}

}  // namespace

}  // namespace content
