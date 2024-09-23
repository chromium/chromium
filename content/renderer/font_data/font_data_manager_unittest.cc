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

    memory_map_region_ =
        base::ReadOnlySharedMemoryRegion::Create(asset->getLength());
    EXPECT_TRUE(memory_map_region_.IsValid());

    size_t bytes_read = asset->read(memory_map_region_.mapping.memory(),
                                    memory_map_region_.mapping.size());
    EXPECT_EQ(bytes_read, asset->getLength());

    base::ReadOnlySharedMemoryRegion region =
        memory_map_region_.region.Duplicate();
    EXPECT_TRUE(region.IsValid());
    result->region = std::move(region);
    result->ttc_index = ttc_index;
    std::move(callback).Run(std::move(result));
  }

  size_t match_family_call_count() const { return match_family_call_count_; }

 private:
  mojo::ReceiverSet<font_data_service::mojom::FontDataService> receivers_;
  size_t match_family_call_count_ = 0;
  base::MappedReadOnlyRegion memory_map_region_;
};

// Wrapper class used to verify SkFontArguments in FontDataManager.
class TestFontDataManager : public font_data_service::FontDataManager {
 public:
  TestFontDataManager(int expected_coordinate_count, int expected_ttc_index)
      : expected_coordinate_count_(expected_coordinate_count),
        expected_ttc_index_(expected_ttc_index) {}
  TestFontDataManager() = delete;
  TestFontDataManager(const TestFontServiceApp&) = delete;
  TestFontDataManager& operator=(const TestFontServiceApp&) = delete;

  sk_sp<SkTypeface> onMakeFromStreamArgs(
      std::unique_ptr<SkStreamAsset> asset,
      const SkFontArguments& font_arguments) const override {
    EXPECT_EQ(expected_coordinate_count_,
              font_arguments.getVariationDesignPosition().coordinateCount);
    EXPECT_EQ(expected_ttc_index_, font_arguments.getCollectionIndex());
    return font_data_service::FontDataManager::onMakeFromStreamArgs(
        std::move(asset), font_arguments);
  }

 private:
  int expected_coordinate_count_;
  int expected_ttc_index_;
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

TEST_F(FontDataManagerUnitTest, FontArgumentTest) {
  // Bahnschrift is a font family with 2 axes hence coordinate count should
  // be 2.
  auto font_manager = sk_make_sp<TestFontDataManager>(2, 0);
  font_manager->SetFontServiceForTesting(
      test_font_data_service_app_.CreateRemote());
  base::cstring_view family_name = "Bahnschrift";
  SkString result_family_name;
  sk_sp<SkTypeface> result = font_manager->matchFamilyStyle(
      family_name.data(), {400, 5, SkFontStyle::kUpright_Slant});
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

using FontDataManagerDeathTest = FontDataManagerUnitTest;

// Methods are unused in FontDataManager.
TEST_F(FontDataManagerDeathTest, CountFamilies) {
  EXPECT_DEATH_IF_SUPPORTED(skia_font_manager_->countFamilies(), "");
}

TEST_F(FontDataManagerDeathTest, GetFamilyName) {
  SkString family_name;
  EXPECT_DEATH_IF_SUPPORTED(skia_font_manager_->getFamilyName(0, &family_name),
                            "");
}

TEST_F(FontDataManagerDeathTest, CreateStyleSet) {
  EXPECT_DEATH_IF_SUPPORTED(skia_font_manager_->createStyleSet(0), "");
}

TEST_F(FontDataManagerDeathTest, MatchFamily) {
  EXPECT_DEATH_IF_SUPPORTED(skia_font_manager_->matchFamily("Segoe UI"), "");
}

TEST_F(FontDataManagerDeathTest, MatchFamilyStyleCharacter) {
  SkFontStyle style(400, 5, SkFontStyle::kUpright_Slant);
  SkUnichar uni_char = 0x0041;  // 'A'
  EXPECT_DEATH_IF_SUPPORTED(skia_font_manager_->matchFamilyStyleCharacter(
                                "Segoe UI", style, nullptr, 0, uni_char),
                            "");
}

}  // namespace

}  // namespace content
