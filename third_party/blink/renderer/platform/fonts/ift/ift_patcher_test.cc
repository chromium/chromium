// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/ift/ift_patcher.h"

#include <memory>
#include <optional>

#include "base/containers/span.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
namespace {

constexpr char kPatchBUrl[] = "00.1.ift_gk";
constexpr char kPatchCUrl[] = "04.1.ift_gk";
constexpr uint32_t kLigaTag = 0x6c696761;  // 'liga'
constexpr uint32_t kWghtTag = 0x77676874;  // 'wght'
constexpr uint32_t kWdthTag = 0x77647468;  // 'wdth'

Vector<uint8_t> LoadTestData(const String& file_name) {
  std::optional<Vector<char>> data =
      test::ReadFromFile(test::PlatformTestDataPath(file_name));
  CHECK(data.has_value());
  return Vector<uint8_t>(base::as_byte_span(*data));
}

// Valid OpenType font without IFT/IFTX tables.
Vector<uint8_t> StandardFontWithoutIft() {
  return LoadTestData("roboto-a.ttf");
}

// Font with initial codepoint for 'a' (97).
Vector<uint8_t> IftFont() {
  return LoadTestData("roboto-ift.ttf");
}

// Glyph-keyed patch for codepoint 'b' (98).
Vector<uint8_t> PatchBPayload() {
  return LoadTestData("00.1.ift_gk");
}

// Glyph-keyed patch for codepoint 'c' (99).
Vector<uint8_t> PatchCPayload() {
  return LoadTestData("04.1.ift_gk");
}

}  // namespace

class IftPatcherTest : public testing::Test {
 protected:
  void SetUp() override {
    scoped_feature_ =
        std::make_unique<ScopedIncrementalFontTransferForTest>(true);
  }

  std::unique_ptr<ScopedIncrementalFontTransferForTest> scoped_feature_;
};

TEST_F(IftPatcherTest, NonIftFontReturnsNull) {
  Vector<uint8_t> font_data = StandardFontWithoutIft();
  EXPECT_EQ(IftPatcher::Create(font_data), nullptr);

  uint8_t malformed_data[] = {0x00, 0x01, 0x02, 0x03, 0x04};
  EXPECT_EQ(IftPatcher::Create(malformed_data), nullptr);

  EXPECT_EQ(IftPatcher::Create(base::span<const uint8_t>()), nullptr);
}

TEST_F(IftPatcherTest, IftFontReturnsIftPatcher) {
  std::unique_ptr<IftPatcher> patcher = IftPatcher::Create(IftFont());
  ASSERT_NE(patcher, nullptr);
  EXPECT_FALSE(patcher->HasPendingPatchRequests());
}

TEST_F(IftPatcherTest, FeatureFlagDisabledReturnsNull) {
  ScopedIncrementalFontTransferForTest disabled_feature(false);
  EXPECT_EQ(IftPatcher::Create(IftFont()), nullptr);
}

TEST_F(IftPatcherTest, UnmodifiedPatcherReturnsOriginalFontBytes) {
  Vector<uint8_t> original_font = IftFont();
  std::unique_ptr<IftPatcher> patcher = IftPatcher::Create(original_font);
  ASSERT_NE(patcher, nullptr);
  base::span<const uint8_t> font_data = patcher->GetFontData();
  EXPECT_EQ(font_data, base::span(original_font));
}

TEST_F(IftPatcherTest, MissingCodepointsReturnsRequiredPatchUrls) {
  std::unique_ptr<IftPatcher> patcher = IftPatcher::Create(IftFont());
  ASSERT_NE(patcher, nullptr);

  IftSubsetDefinition subset;
  subset.AddCodepoint('b');
  Vector<String> patches = patcher->RequestPatches(subset);
  EXPECT_THAT(patches, testing::ElementsAre(kPatchBUrl));
}

TEST_F(IftPatcherTest, ApplyPatchesUpdatesFontData) {
  Vector<uint8_t> original_font = IftFont();
  std::unique_ptr<IftPatcher> patcher = IftPatcher::Create(original_font);
  ASSERT_NE(patcher, nullptr);

  IftSubsetDefinition subset;
  subset.AddCodepoint('b');
  Vector<String> patches = patcher->RequestPatches(subset);
  ASSERT_THAT(patches, testing::ElementsAre(kPatchBUrl));

  patcher->AddPatchData(patches[0], PatchBPayload());
  EXPECT_EQ(patcher->ApplyPatches(subset), IftPatcher::ApplyStatus::kSuccess);
  EXPECT_FALSE(patcher->GetFontData().empty());
  EXPECT_NE(patcher->GetFontData().size(), original_font.size());
  EXPECT_FALSE(patcher->HasPendingPatchRequests());
}

TEST_F(IftPatcherTest, ApplyPatchesSequentiallyUpdatesFontData) {
  Vector<uint8_t> original_font = IftFont();
  std::unique_ptr<IftPatcher> patcher = IftPatcher::Create(original_font);
  ASSERT_NE(patcher, nullptr);

  IftSubsetDefinition subset_b;
  subset_b.AddCodepoint('b');
  Vector<String> patches_b = patcher->RequestPatches(subset_b);
  ASSERT_THAT(patches_b, testing::ElementsAre(kPatchBUrl));

  patcher->AddPatchData(patches_b[0], PatchBPayload());
  EXPECT_EQ(patcher->ApplyPatches(subset_b), IftPatcher::ApplyStatus::kSuccess);
  Vector<uint8_t> font_data_b(patcher->GetFontData());
  EXPECT_NE(font_data_b, original_font);
  EXPECT_FALSE(patcher->HasPendingPatchRequests());

  IftSubsetDefinition subset_c;
  subset_c.AddCodepoint('c');
  Vector<String> patches_c = patcher->RequestPatches(subset_c);
  ASSERT_THAT(patches_c, testing::ElementsAre(kPatchCUrl));

  patcher->AddPatchData(patches_c[0], PatchCPayload());
  EXPECT_EQ(patcher->ApplyPatches(subset_c), IftPatcher::ApplyStatus::kSuccess);
  base::span<const uint8_t> font_data_c = patcher->GetFontData();
  EXPECT_NE(font_data_c, base::span(font_data_b));
  EXPECT_FALSE(patcher->HasPendingPatchRequests());

  IftSubsetDefinition subset_both = subset_b;
  subset_both.Union(subset_c);
  EXPECT_TRUE(patcher->RequestPatches(subset_both).empty());
  EXPECT_EQ(patcher->ApplyPatches(subset_both),
            IftPatcher::ApplyStatus::kSuccess);
}

TEST_F(IftPatcherTest, ApplyPatchesWithoutRequiredDataReturnsMissingPatches) {
  std::unique_ptr<IftPatcher> patcher = IftPatcher::Create(IftFont());
  ASSERT_NE(patcher, nullptr);

  IftSubsetDefinition subset;
  subset.AddCodepoint('b');
  Vector<String> patches = patcher->RequestPatches(subset);
  ASSERT_THAT(patches, testing::ElementsAre(kPatchBUrl));

  EXPECT_EQ(patcher->ApplyPatches(subset),
            IftPatcher::ApplyStatus::kMissingPatches);
}

TEST_F(IftPatcherTest, ApplyPatchesWithCorruptedPayloadReturnsPatchError) {
  std::unique_ptr<IftPatcher> patcher = IftPatcher::Create(IftFont());
  ASSERT_NE(patcher, nullptr);

  IftSubsetDefinition subset;
  subset.AddCodepoint('b');
  Vector<String> patches = patcher->RequestPatches(subset);
  ASSERT_THAT(patches, testing::ElementsAre(kPatchBUrl));

  uint8_t invalid_payload[] = {0xDE, 0xAD, 0xBE, 0xEF};
  patcher->AddPatchData(patches[0], invalid_payload);
  EXPECT_EQ(patcher->ApplyPatches(subset),
            IftPatcher::ApplyStatus::kPatchError);
}

TEST_F(IftPatcherTest, RequestPatchesCreatesPendingPatchRequests) {
  std::unique_ptr<IftPatcher> patcher = IftPatcher::Create(IftFont());
  ASSERT_NE(patcher, nullptr);
  EXPECT_FALSE(patcher->HasPendingPatchRequests());

  IftSubsetDefinition subset;
  subset.AddCodepoint('b');
  Vector<String> patches = patcher->RequestPatches(subset);
  ASSERT_THAT(patches, testing::ElementsAre(kPatchBUrl));
  EXPECT_TRUE(patcher->HasPendingPatchRequests());

  patcher->AddPatchData(patches[0], PatchBPayload());
  EXPECT_FALSE(patcher->HasPendingPatchRequests());
}

TEST_F(IftPatcherTest, DuplicateOrPreviouslyRequestedPatchesAreOmitted) {
  std::unique_ptr<IftPatcher> patcher = IftPatcher::Create(IftFont());
  ASSERT_NE(patcher, nullptr);

  IftSubsetDefinition subset;
  subset.AddCodepoint('b');
  Vector<String> patches_first = patcher->RequestPatches(subset);
  EXPECT_THAT(patches_first, testing::ElementsAre(kPatchBUrl));

  Vector<String> patches_second = patcher->RequestPatches(subset);
  EXPECT_TRUE(patches_second.empty());
}

TEST_F(IftPatcherTest, AlreadyCoveredOrEmptySubsetReturnsNoPatches) {
  std::unique_ptr<IftPatcher> patcher = IftPatcher::Create(IftFont());
  ASSERT_NE(patcher, nullptr);

  IftSubsetDefinition empty_subset;
  EXPECT_TRUE(patcher->RequestPatches(empty_subset).empty());
  EXPECT_EQ(patcher->ApplyPatches(empty_subset),
            IftPatcher::ApplyStatus::kSuccess);

  // 'a' (codepoint 97) is an initial codepoint embedded in the base font.
  IftSubsetDefinition covered_subset;
  covered_subset.AddCodepoint('a');
  EXPECT_TRUE(patcher->RequestPatches(covered_subset).empty());
  EXPECT_EQ(patcher->ApplyPatches(covered_subset),
            IftPatcher::ApplyStatus::kSuccess);
  EXPECT_FALSE(patcher->HasPendingPatchRequests());
}

TEST_F(IftPatcherTest, SubsetDefinitionUnionCombinesSubsets) {
  IftSubsetDefinition subset;
  subset.AddCodepoint('b');

  IftSubsetDefinition other_subset;
  other_subset.AddCodepoint('c');
  subset.Union(other_subset);

  std::unique_ptr<IftPatcher> patcher = IftPatcher::Create(IftFont());
  ASSERT_NE(patcher, nullptr);

  Vector<String> patches = patcher->RequestPatches(subset);
  EXPECT_THAT(patches, testing::UnorderedElementsAre(kPatchBUrl, kPatchCUrl));
}

TEST_F(IftPatcherTest, FeatureTagsAndDesignSpaceCanBeAddedToSubset) {
  std::unique_ptr<IftPatcher> patcher = IftPatcher::Create(IftFont());
  ASSERT_NE(patcher, nullptr);

  IftSubsetDefinition subset;
  subset.AddCodepoint('b');
  subset.AddFeatureTag(0x6c696761);                 // 'liga'
  subset.AddDesignSpace(0x77676874, 100.0, 900.0);  // 'wght'
  subset.AddDesignSpace(0x77647468, 75.0, 125.0);   // 'wdth'

  Vector<String> patches = patcher->RequestPatches(subset);
  EXPECT_FALSE(patches.empty());
}

TEST_F(IftPatcherTest, SubsetDefinitionModificationReturnsTrueOnlyWhenChanged) {
  IftSubsetDefinition subset;

  EXPECT_TRUE(subset.AddCodepoint('e'));
  EXPECT_FALSE(subset.AddCodepoint('e'));

  EXPECT_TRUE(subset.AddFeatureTag(0x6b65726e));
  EXPECT_FALSE(subset.AddFeatureTag(0x6b65726e));

  EXPECT_TRUE(subset.AddDesignSpace(0x77676874, 100.0, 400.0));
  EXPECT_FALSE(subset.AddDesignSpace(0x77676874, 200.0, 300.0));
  EXPECT_TRUE(
      subset.AddDesignSpace(0x77676874, 400.0, 700.0));  // expands range
}

TEST_F(IftPatcherTest, SubsetDefinitionUnionMergesCodepoints) {
  IftSubsetDefinition target;
  IftSubsetDefinition other;
  other.AddCodepoint('b');

  target.Union(other);

  EXPECT_FALSE(target.AddCodepoint('b'));
}

TEST_F(IftPatcherTest, SubsetDefinitionUnionMergesFeatureTags) {
  IftSubsetDefinition target;
  IftSubsetDefinition other;
  other.AddFeatureTag(kLigaTag);

  target.Union(other);

  EXPECT_FALSE(target.AddFeatureTag(kLigaTag));
}

TEST_F(IftPatcherTest, SubsetDefinitionUnionMergesDesignSpace) {
  IftSubsetDefinition target;
  target.AddDesignSpace(kWghtTag, 100.0, 400.0);
  IftSubsetDefinition other;
  other.AddDesignSpace(kWghtTag, 300.0, 700.0);
  other.AddDesignSpace(kWdthTag, 75.0, 125.0);

  target.Union(other);

  EXPECT_FALSE(target.AddDesignSpace(kWghtTag, 400.0, 700.0));
  EXPECT_FALSE(target.AddDesignSpace(kWdthTag, 75.0, 125.0));
}

}  // namespace blink
