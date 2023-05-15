// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/crop_id_web_contents_helper.h"

#include <memory>

#include "base/uuid.h"
#include "build/build_config.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#error Region Capture not supported on Android.
#endif

namespace content {

namespace {

MATCHER(IsEmptyCropId, "") {
  static_assert(std::is_same<decltype(arg), const std::string&>::value, "");
  return arg.empty();
}

MATCHER(IsValidCropId, "") {
  static_assert(std::is_same<decltype(arg), const std::string&>::value, "");
  return base::Uuid::ParseLowercase(arg).is_valid();
}

}  // namespace

class CropIdWebContentsHelperTest : public RenderViewHostImplTestHarness {
 public:
  ~CropIdWebContentsHelperTest() override = default;

  void SetUp() override { RenderViewHostImplTestHarness::SetUp(); }

  std::unique_ptr<TestWebContents> MakeTestWebContents() {
    scoped_refptr<SiteInstance> instance =
        SiteInstance::Create(GetBrowserContext());
    instance->GetProcess()->Init();

    return TestWebContents::Create(GetBrowserContext(), std::move(instance));
  }

  static CropIdWebContentsHelper* helper(WebContents* web_contents) {
    // No-op if already created.
    CropIdWebContentsHelper::CreateForWebContents(web_contents);
    return CropIdWebContentsHelper::FromWebContents(web_contents);
  }

  static base::Token GUIDToToken(const base::Uuid& guid) {
    return CropIdWebContentsHelper::GUIDToToken(guid);
  }
};

TEST_F(CropIdWebContentsHelperTest,
       IsAssociatedWithCropIdReturnsFalseForUnknownCropId) {
  const std::unique_ptr<TestWebContents> web_contents = MakeTestWebContents();
  CropIdWebContentsHelper::CreateForWebContents(web_contents.get());
  auto* helper = CropIdWebContentsHelper::FromWebContents(web_contents.get());
  ASSERT_NE(helper, nullptr);

  // Test focus.
  const base::Uuid unknown_crop_id = base::Uuid::GenerateRandomV4();
  EXPECT_FALSE(helper->IsAssociatedWithCropId(GUIDToToken(unknown_crop_id)));

  // Extra-test: Ensure the query above did not accidentally record
  // `unknown_crop_id` as a known crop-ID.
  EXPECT_FALSE(helper->IsAssociatedWithCropId(GUIDToToken(unknown_crop_id)));
}

TEST_F(CropIdWebContentsHelperTest, ProduceCropIdReturnsCropId) {
  const std::unique_ptr<TestWebContents> web_contents = MakeTestWebContents();
  CropIdWebContentsHelper::CreateForWebContents(web_contents.get());
  auto* helper = CropIdWebContentsHelper::FromWebContents(web_contents.get());
  ASSERT_NE(helper, nullptr);

  EXPECT_THAT(helper->ProduceCropId(), IsValidCropId());
}

TEST_F(CropIdWebContentsHelperTest,
       IsAssociatedWithCropIdReturnsTrueForKnownCropIdIfCorrectWebContents) {
  const std::unique_ptr<TestWebContents> web_contents = MakeTestWebContents();
  CropIdWebContentsHelper::CreateForWebContents(web_contents.get());
  auto* helper = CropIdWebContentsHelper::FromWebContents(web_contents.get());
  ASSERT_NE(helper, nullptr);

  const std::unique_ptr<TestWebContents> other_web_contents =
      MakeTestWebContents();
  CropIdWebContentsHelper::CreateForWebContents(other_web_contents.get());
  auto* other_helper =
      CropIdWebContentsHelper::FromWebContents(other_web_contents.get());
  ASSERT_NE(other_helper, nullptr);

  const std::string crop_id_str = helper->ProduceCropId();
  EXPECT_THAT(crop_id_str, IsValidCropId());

  const base::Token crop_id =
      GUIDToToken(base::Uuid::ParseLowercase(crop_id_str));

  EXPECT_TRUE(helper->IsAssociatedWithCropId(crop_id));
  EXPECT_FALSE(other_helper->IsAssociatedWithCropId(crop_id));
}

TEST_F(CropIdWebContentsHelperTest, MaxCropIdsPerWebContentsObserved) {
  const std::unique_ptr<TestWebContents> web_contents[2] = {
      MakeTestWebContents(), MakeTestWebContents()};
  CropIdWebContentsHelper::CreateForWebContents(web_contents[0].get());
  CropIdWebContentsHelper::CreateForWebContents(web_contents[1].get());
  CropIdWebContentsHelper* helpers[2] = {
      CropIdWebContentsHelper::FromWebContents(web_contents[0].get()),
      CropIdWebContentsHelper::FromWebContents(web_contents[1].get())};

  std::string crop_ids_str[2]
                          [CropIdWebContentsHelper::kMaxCropIdsPerWebContents];

  // Up to `kMaxCropIdsPerWebContents` allowed on each WebContents.
  for (size_t web_contents_idx = 0; web_contents_idx < 2; ++web_contents_idx) {
    for (size_t i = 0; i < CropIdWebContentsHelper::kMaxCropIdsPerWebContents;
         ++i) {
      crop_ids_str[web_contents_idx][i] =
          helpers[web_contents_idx]->ProduceCropId();
      EXPECT_THAT(crop_ids_str[web_contents_idx][i], IsValidCropId());
    }
  }

  // Attempts to produce more crop-IDs on either WebContents fail.
  for (CropIdWebContentsHelper* helper : helpers) {
    EXPECT_THAT(helper->ProduceCropId(), IsEmptyCropId());
  }

  // The original crop-IDs are not forgotten.
  for (size_t web_contents_idx = 0; web_contents_idx < 2; ++web_contents_idx) {
    for (size_t i = 0; i < CropIdWebContentsHelper::kMaxCropIdsPerWebContents;
         ++i) {
      const base::Token crop_id = GUIDToToken(
          base::Uuid::ParseLowercase(crop_ids_str[web_contents_idx][i]));
      EXPECT_TRUE(helpers[web_contents_idx]->IsAssociatedWithCropId(crop_id));

      // Extra-test: They're also still associated *only* with the relevant WC.
      const size_t other_web_contents_idx = 1 - web_contents_idx;
      EXPECT_FALSE(
          helpers[other_web_contents_idx]->IsAssociatedWithCropId(crop_id));
    }
  }
}

TEST_F(CropIdWebContentsHelperTest,
       CrossDocumentNavigationClearsCropIdsAssociation) {
  std::unique_ptr<TestWebContents> web_contents = MakeTestWebContents();
  CropIdWebContentsHelper::CreateForWebContents(web_contents.get());
  auto* helper = CropIdWebContentsHelper::FromWebContents(web_contents.get());
  ASSERT_NE(helper, nullptr);

  // Setup - WebContents navigated to a document, crop-ID produced.
  web_contents->NavigateAndCommit(GURL("https://tests-r-us.com/first.html"));
  base::Token crop_id_1;
  {
    const std::string crop_id_str = helper->ProduceCropId();
    ASSERT_THAT(crop_id_str, IsValidCropId());
    crop_id_1 = GUIDToToken(base::Uuid::ParseLowercase(crop_id_str));
  }
  ASSERT_TRUE(helper->IsAssociatedWithCropId(crop_id_1));  // Sanity-check.

  // Cross-document navigation occurs.
  web_contents->NavigateAndCommit(GURL("https://tests-r-us.com/second.html"));

  // Verification #1: The old crop-ID is forgotten.
  ASSERT_FALSE(helper->IsAssociatedWithCropId(crop_id_1));

  // Verification #2: New crop-IDs may be recorded.
  {
    const std::string crop_id_str = helper->ProduceCropId();
    EXPECT_THAT(crop_id_str, IsValidCropId());
    const base::Token crop_id_2 =
        GUIDToToken(base::Uuid::ParseLowercase(crop_id_str));
    ASSERT_TRUE(helper->IsAssociatedWithCropId(crop_id_2));  // Sanity-check.
  }

  // Verification #3: The forgotten crop-ID is not counted against the limit
  // of crop-IDs applied to a WebContents. (kMaxCropIdsPerWebContents - 1 more
  // invocations allowed, then the next one fails.)
  for (size_t i = 0; i < CropIdWebContentsHelper::kMaxCropIdsPerWebContents - 1;
       ++i) {
    EXPECT_THAT(helper->ProduceCropId(), IsValidCropId());
  }
  EXPECT_THAT(helper->ProduceCropId(), IsEmptyCropId());
}

TEST_F(CropIdWebContentsHelperTest,
       InDocumentNavigationDoesNotClearCropIdsAssociation) {
  std::unique_ptr<TestWebContents> web_contents = MakeTestWebContents();
  CropIdWebContentsHelper::CreateForWebContents(web_contents.get());
  auto* helper = CropIdWebContentsHelper::FromWebContents(web_contents.get());
  ASSERT_NE(helper, nullptr);

  // Setup - WebContents navigated to a document, crop-ID produced.
  web_contents->NavigateAndCommit(GURL("https://tests-r-us.com/first.html"));
  const std::string crop_id_str = helper->ProduceCropId();
  ASSERT_THAT(crop_id_str, IsValidCropId());
  const base::Token crop_id =
      GUIDToToken(base::Uuid::ParseLowercase(crop_id_str));

  // Test sanity-check.
  ASSERT_TRUE(helper->IsAssociatedWithCropId(crop_id));

  // In-document navigation occurs. The crop-ID is not forgotten.
  web_contents->NavigateAndCommit(GURL("https://tests-r-us.com/first.html#a"));
  EXPECT_TRUE(helper->IsAssociatedWithCropId(crop_id));
}

}  // namespace content
