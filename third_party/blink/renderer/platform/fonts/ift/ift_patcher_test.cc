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

// Patch URLs embedded in `roboto-ift.ttf`. The base font only supports 'a'.
// Supporting 'b' or 'c' requires a table-keyed patch (cmap, hmtx, etc.) and a
// glyph-keyed patch (outlines). Table-keyed patches are invalidating, so which
// one is needed depends on the patches already applied.
//
// Table-keyed patch for 'b' applied to the base font.
constexpr char kPatchBTableUrl[] = "0K.ift_tk";
// Table-keyed patch for 'c' applied after `kPatchBTableUrl`.
constexpr char kPatchCAfterBTableUrl[] = "0C.ift_tk";
// Glyph-keyed patch with the outline for 'b'.
constexpr char kPatchBGlyphUrl[] = "04.1.ift_gk";
// Glyph-keyed patch with the outline for 'c'.
constexpr char kPatchCGlyphUrl[] = "08.1.ift_gk";
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

// Font with initial codepoint for 'a' (97). See
// `third_party/blink/renderer/platform/testing/data/third_party/RobotoClassic/README.chromium`.
Vector<uint8_t> IftFont() {
  return LoadTestData("roboto-ift.ttf");
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
  EXPECT_THAT(patches,
              testing::UnorderedElementsAre(kPatchBTableUrl, kPatchBGlyphUrl));
}

TEST_F(IftPatcherTest, ApplyPatchesUpdatesFontData) {
  Vector<uint8_t> original_font = IftFont();
  std::unique_ptr<IftPatcher> patcher = IftPatcher::Create(original_font);
  ASSERT_NE(patcher, nullptr);

  IftSubsetDefinition subset;
  subset.AddCodepoint('b');
  Vector<String> patches = patcher->RequestPatches(subset);
  ASSERT_THAT(patches,
              testing::UnorderedElementsAre(kPatchBTableUrl, kPatchBGlyphUrl));

  patcher->AddPatchData(kPatchBTableUrl, LoadTestData(kPatchBTableUrl));
  patcher->AddPatchData(kPatchBGlyphUrl, LoadTestData(kPatchBGlyphUrl));
  EXPECT_EQ(patcher->ApplyPatches(subset), IftPatcher::ApplyStatus::kSuccess);
  EXPECT_FALSE(patcher->GetFontData().empty());
  EXPECT_NE(patcher->GetFontData().size(), original_font.size());
  EXPECT_FALSE(patcher->HasPendingPatchRequests());
  EXPECT_TRUE(patcher->RequestPatches(subset).empty());
}

TEST_F(IftPatcherTest, RenderedFontRequiresPatchesUntilMarkedRendered) {
  std::unique_ptr<IftPatcher> patcher = IftPatcher::Create(IftFont());
  ASSERT_NE(patcher, nullptr);

  IftSubsetDefinition subset;
  subset.AddCodepoint('b');
  EXPECT_FALSE(patcher->HasUnrenderedChanges());
  EXPECT_TRUE(patcher->RenderedFontRequiresPatches(subset));
  ASSERT_THAT(patcher->RequestPatches(subset),
              testing::UnorderedElementsAre(kPatchBTableUrl, kPatchBGlyphUrl));
  patcher->AddPatchData(kPatchBTableUrl, LoadTestData(kPatchBTableUrl));
  patcher->AddPatchData(kPatchBGlyphUrl, LoadTestData(kPatchBGlyphUrl));
  ASSERT_EQ(patcher->ApplyPatches(subset), IftPatcher::ApplyStatus::kSuccess);

  // The patches are applied, but the patched font data is not rendered yet.
  EXPECT_TRUE(patcher->RenderedFontRequiresPatches(subset));
  EXPECT_TRUE(patcher->HasUnrenderedChanges());

  patcher->MarkFontDataRendered();
  EXPECT_FALSE(patcher->RenderedFontRequiresPatches(subset));
  EXPECT_FALSE(patcher->HasUnrenderedChanges());
}

TEST_F(IftPatcherTest, ApplyPatchesSequentiallyUpdatesFontData) {
  Vector<uint8_t> original_font = IftFont();
  std::unique_ptr<IftPatcher> patcher = IftPatcher::Create(original_font);
  ASSERT_NE(patcher, nullptr);

  IftSubsetDefinition subset_b;
  subset_b.AddCodepoint('b');
  Vector<String> patches_b = patcher->RequestPatches(subset_b);
  ASSERT_THAT(patches_b,
              testing::UnorderedElementsAre(kPatchBTableUrl, kPatchBGlyphUrl));

  patcher->AddPatchData(kPatchBTableUrl, LoadTestData(kPatchBTableUrl));
  patcher->AddPatchData(kPatchBGlyphUrl, LoadTestData(kPatchBGlyphUrl));
  EXPECT_EQ(patcher->ApplyPatches(subset_b), IftPatcher::ApplyStatus::kSuccess);
  Vector<uint8_t> font_data_b(patcher->GetFontData());
  EXPECT_NE(font_data_b, original_font);
  EXPECT_FALSE(patcher->HasPendingPatchRequests());

  // Applying the table-keyed patch for 'b' replaced the available table-keyed
  // patches, so 'c' now needs a different one than it would from the base
  // font.
  IftSubsetDefinition subset_c;
  subset_c.AddCodepoint('c');
  Vector<String> patches_c = patcher->RequestPatches(subset_c);
  ASSERT_THAT(patches_c, testing::UnorderedElementsAre(kPatchCAfterBTableUrl,
                                                       kPatchCGlyphUrl));

  patcher->AddPatchData(kPatchCAfterBTableUrl,
                        LoadTestData(kPatchCAfterBTableUrl));
  patcher->AddPatchData(kPatchCGlyphUrl, LoadTestData(kPatchCGlyphUrl));
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
  ASSERT_THAT(patches,
              testing::UnorderedElementsAre(kPatchBTableUrl, kPatchBGlyphUrl));

  EXPECT_EQ(patcher->ApplyPatches(subset),
            IftPatcher::ApplyStatus::kMissingPatches);
}

TEST_F(IftPatcherTest, ApplyPatchesWithCorruptedPayloadReturnsPatchError) {
  std::unique_ptr<IftPatcher> patcher = IftPatcher::Create(IftFont());
  ASSERT_NE(patcher, nullptr);

  IftSubsetDefinition subset;
  subset.AddCodepoint('b');
  Vector<String> patches = patcher->RequestPatches(subset);
  ASSERT_THAT(patches,
              testing::UnorderedElementsAre(kPatchBTableUrl, kPatchBGlyphUrl));

  uint8_t invalid_payload[] = {0xDE, 0xAD, 0xBE, 0xEF};
  for (const String& patch : patches) {
    patcher->AddPatchData(patch, invalid_payload);
  }
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
  ASSERT_THAT(patches,
              testing::UnorderedElementsAre(kPatchBTableUrl, kPatchBGlyphUrl));
  EXPECT_TRUE(patcher->HasPendingPatchRequests());

  patcher->AddPatchData(kPatchBTableUrl, LoadTestData(kPatchBTableUrl));
  EXPECT_TRUE(patcher->HasPendingPatchRequests());

  patcher->AddPatchData(kPatchBGlyphUrl, LoadTestData(kPatchBGlyphUrl));
  EXPECT_FALSE(patcher->HasPendingPatchRequests());
}

TEST_F(IftPatcherTest, DuplicateOrPreviouslyRequestedPatchesAreOmitted) {
  std::unique_ptr<IftPatcher> patcher = IftPatcher::Create(IftFont());
  ASSERT_NE(patcher, nullptr);

  IftSubsetDefinition subset;
  subset.AddCodepoint('b');
  Vector<String> patches_first = patcher->RequestPatches(subset);
  EXPECT_THAT(patches_first,
              testing::UnorderedElementsAre(kPatchBTableUrl, kPatchBGlyphUrl));

  Vector<String> patches_second = patcher->RequestPatches(subset);
  EXPECT_TRUE(patches_second.empty());
}

TEST_F(IftPatcherTest, AlreadyCoveredOrEmptySubsetReturnsNoPatches) {
  std::unique_ptr<IftPatcher> patcher = IftPatcher::Create(IftFont());
  ASSERT_NE(patcher, nullptr);

  IftSubsetDefinition empty_subset;
  EXPECT_FALSE(patcher->RenderedFontRequiresPatches(empty_subset));
  EXPECT_TRUE(patcher->RequestPatches(empty_subset).empty());
  EXPECT_EQ(patcher->ApplyPatches(empty_subset),
            IftPatcher::ApplyStatus::kSuccess);

  // 'a' (codepoint 97) is an initial codepoint embedded in the base font.
  IftSubsetDefinition covered_subset;
  covered_subset.AddCodepoint('a');
  EXPECT_FALSE(patcher->RenderedFontRequiresPatches(covered_subset));
  EXPECT_TRUE(patcher->RequestPatches(covered_subset).empty());
  EXPECT_EQ(patcher->ApplyPatches(covered_subset),
            IftPatcher::ApplyStatus::kSuccess);
  EXPECT_FALSE(patcher->HasPendingPatchRequests());
}

TEST_F(IftPatcherTest, UnpatchableSubsetRequiresNoPatches) {
  std::unique_ptr<IftPatcher> patcher = IftPatcher::Create(IftFont());
  ASSERT_NE(patcher, nullptr);

  // 'z' is neither in the base font nor in any patch entry.
  IftSubsetDefinition unpatchable_subset;
  unpatchable_subset.AddCodepoint('z');
  EXPECT_FALSE(patcher->RenderedFontRequiresPatches(unpatchable_subset));
  EXPECT_TRUE(patcher->RequestPatches(unpatchable_subset).empty());
  EXPECT_EQ(patcher->ApplyPatches(unpatchable_subset),
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

  // Both glyph-keyed patches are requested, but only one of the table-keyed
  // patches since they are mutually exclusive.
  Vector<String> patches = patcher->RequestPatches(subset);
  EXPECT_THAT(patches, testing::UnorderedElementsAre(
                           kPatchBTableUrl, kPatchBGlyphUrl, kPatchCGlyphUrl));
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

TEST_F(IftPatcherTest, SubsetDefinitionClearResetsSubset) {
  IftSubsetDefinition subset;
  subset.AddCodepoint('b');
  subset.AddFeatureTag(kLigaTag);
  subset.AddDesignSpace(kWghtTag, 100.0, 400.0);

  subset.Clear();

  EXPECT_TRUE(subset.AddCodepoint('b'));
  EXPECT_TRUE(subset.AddFeatureTag(kLigaTag));
  EXPECT_TRUE(subset.AddDesignSpace(kWghtTag, 100.0, 400.0));
}

}  // namespace blink
