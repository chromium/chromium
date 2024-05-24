// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/sub_capture_target_id_web_contents_helper.h"

#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

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

using ::testing::Values;
using ::testing::WithParamInterface;

using Type = SubCaptureTargetIdWebContentsHelper::Type;

constexpr size_t kMaxIdsPerWebContents =
    SubCaptureTargetIdWebContentsHelper::kMaxIdsPerWebContents;

MATCHER(IsEmptyId, "") {
  static_assert(std::is_same<decltype(arg), const std::string&>::value, "");
  return arg.empty();
}

MATCHER(IsValidId, "") {
  static_assert(std::is_same<decltype(arg), const std::string&>::value, "");
  return base::Uuid::ParseLowercase(arg).is_valid();
}

}  // namespace

class SubCaptureTargetIdWebContentsHelperTest
    : public RenderViewHostImplTestHarness,
      public WithParamInterface<Type> {
 public:
  SubCaptureTargetIdWebContentsHelperTest() : type_(GetParam()) {}
  ~SubCaptureTargetIdWebContentsHelperTest() override = default;

  void SetUp() override { RenderViewHostImplTestHarness::SetUp(); }

  std::unique_ptr<TestWebContents> MakeTestWebContents() {
    scoped_refptr<SiteInstance> instance =
        SiteInstance::Create(GetBrowserContext());
    instance->GetProcess()->Init();

    return TestWebContents::Create(GetBrowserContext(), std::move(instance));
  }

  static SubCaptureTargetIdWebContentsHelper* MakeHelper(
      WebContents* web_contents) {
    // No-op if already created.
    SubCaptureTargetIdWebContentsHelper::CreateForWebContents(web_contents);
    return SubCaptureTargetIdWebContentsHelper::FromWebContents(web_contents);
  }

  static base::Token GUIDToToken(const base::Uuid& guid) {
    return SubCaptureTargetIdWebContentsHelper::GUIDToToken(guid);
  }

 protected:
  const Type type_;
};

INSTANTIATE_TEST_SUITE_P(_,
                         SubCaptureTargetIdWebContentsHelperTest,
                         Values(Type::kCropTarget, Type::kRestrictionTarget));

TEST_P(SubCaptureTargetIdWebContentsHelperTest,
       IsAssociatedWithReturnsFalseForUnknownId) {
  const std::unique_ptr<TestWebContents> web_contents = MakeTestWebContents();
  SubCaptureTargetIdWebContentsHelper::CreateForWebContents(web_contents.get());
  auto* helper =
      SubCaptureTargetIdWebContentsHelper::FromWebContents(web_contents.get());
  ASSERT_NE(helper, nullptr);

  // Test focus.
  const base::Uuid unknown_id = base::Uuid::GenerateRandomV4();
  EXPECT_FALSE(helper->IsAssociatedWith(GUIDToToken(unknown_id), type_));

  // Extra-test: Ensure the query above did not accidentally record
  // `unknown_id` as a known ID.
  EXPECT_FALSE(helper->IsAssociatedWith(GUIDToToken(unknown_id), type_));
}

TEST_P(SubCaptureTargetIdWebContentsHelperTest, ProduceIdReturnsId) {
  const std::unique_ptr<TestWebContents> web_contents = MakeTestWebContents();
  SubCaptureTargetIdWebContentsHelper::CreateForWebContents(web_contents.get());
  auto* helper =
      SubCaptureTargetIdWebContentsHelper::FromWebContents(web_contents.get());
  ASSERT_NE(helper, nullptr);

  EXPECT_THAT(helper->ProduceId(type_), IsValidId());
}

TEST_P(SubCaptureTargetIdWebContentsHelperTest,
       IsAssociatedWithReturnsTrueForKnownIdIfCorrectWebContents) {
  const std::unique_ptr<TestWebContents> web_contents = MakeTestWebContents();
  SubCaptureTargetIdWebContentsHelper::CreateForWebContents(web_contents.get());
  auto* helper =
      SubCaptureTargetIdWebContentsHelper::FromWebContents(web_contents.get());
  ASSERT_NE(helper, nullptr);

  const std::unique_ptr<TestWebContents> other_web_contents =
      MakeTestWebContents();
  SubCaptureTargetIdWebContentsHelper::CreateForWebContents(
      other_web_contents.get());
  auto* other_helper = SubCaptureTargetIdWebContentsHelper::FromWebContents(
      other_web_contents.get());
  ASSERT_NE(other_helper, nullptr);

  const std::string id_str = helper->ProduceId(type_);
  EXPECT_THAT(id_str, IsValidId());

  const base::Token id = GUIDToToken(base::Uuid::ParseLowercase(id_str));

  EXPECT_TRUE(helper->IsAssociatedWith(id, type_));
  EXPECT_FALSE(other_helper->IsAssociatedWith(id, type_));
}

TEST_P(SubCaptureTargetIdWebContentsHelperTest, IsAssociatedWithObservesType) {
  const std::unique_ptr<TestWebContents> web_contents = MakeTestWebContents();
  SubCaptureTargetIdWebContentsHelper* helper = MakeHelper(web_contents.get());

  const std::string id_str = helper->ProduceId(type_);
  ASSERT_THAT(id_str, IsValidId());

  const base::Token id = GUIDToToken(base::Uuid::ParseLowercase(id_str));
  ASSERT_TRUE(helper->IsAssociatedWith(id, type_));

  const Type other_type = (type_ == Type::kCropTarget)
                              ? Type::kRestrictionTarget
                              : Type::kCropTarget;
  EXPECT_FALSE(helper->IsAssociatedWith(id, other_type));
}

// Test that the limit of `kMaxIdsPerWebContents` is applied independently
// for different WebContents.
TEST_P(SubCaptureTargetIdWebContentsHelperTest, MaxIdsPerWebContentsObserved) {
  auto web_contents = std::to_array<std::unique_ptr<TestWebContents>>(
      {MakeTestWebContents(), MakeTestWebContents()});
  SubCaptureTargetIdWebContentsHelper::CreateForWebContents(
      web_contents[0].get());
  SubCaptureTargetIdWebContentsHelper::CreateForWebContents(
      web_contents[1].get());
  auto helpers = std::to_array<SubCaptureTargetIdWebContentsHelper*>(
      {SubCaptureTargetIdWebContentsHelper::FromWebContents(
           web_contents[0].get()),
       SubCaptureTargetIdWebContentsHelper::FromWebContents(
           web_contents[1].get())});

  std::array<std::array<std::string, kMaxIdsPerWebContents>, 2> ids_str;

  // Up to `kMaxIdsPerWebContents` allowed on each WebContents.
  for (size_t web_contents_idx = 0; web_contents_idx < 2; ++web_contents_idx) {
    for (size_t i = 0; i < kMaxIdsPerWebContents; ++i) {
      ids_str[web_contents_idx][i] =
          helpers[web_contents_idx]->ProduceId(type_);
      EXPECT_THAT(ids_str[web_contents_idx][i], IsValidId());
    }
  }

  // Attempts to produce more IDs on either WebContents fail.
  for (SubCaptureTargetIdWebContentsHelper* helper : helpers) {
    EXPECT_THAT(helper->ProduceId(type_), IsEmptyId());
  }

  // The original IDs are not forgotten.
  for (size_t web_contents_idx = 0; web_contents_idx < 2; ++web_contents_idx) {
    for (size_t i = 0; i < kMaxIdsPerWebContents; ++i) {
      const base::Token id =
          GUIDToToken(base::Uuid::ParseLowercase(ids_str[web_contents_idx][i]));
      EXPECT_TRUE(helpers[web_contents_idx]->IsAssociatedWith(id, type_));

      // Extra-test: They're also still associated *only* with the relevant WC.
      const size_t other_web_contents_idx = 1 - web_contents_idx;
      EXPECT_FALSE(
          helpers[other_web_contents_idx]->IsAssociatedWith(id, type_));
    }
  }
}

// Test that the limit of `kMaxIdsPerWebContents` is applied independently
// for different types (CropTarget, RestrictionTarget).
TEST_P(SubCaptureTargetIdWebContentsHelperTest, MaxIdsPerTypeObserved) {
  // This switch is no-op. It's only here to make the compiler emit a warning if
  // a developer ever adds values to the Type enum without updating this test.
  // Future developer, please update the container below to cover all types.
  switch (Type()) {
    case Type::kCropTarget:
    case Type::kRestrictionTarget:
      break;
  }
  const std::vector<Type> kAllTypes = {Type::kCropTarget,
                                       Type::kRestrictionTarget};

  // Note that this test does not need to be parameterized, but the overhead
  // of separating it from the rest of the suite is not worth the savings.
  // We therefore skip iterations of this test past the first possible value.
  if (type_ != kAllTypes[0]) {
    GTEST_SKIP();
  }

  std::unique_ptr<TestWebContents> web_contents = MakeTestWebContents();
  SubCaptureTargetIdWebContentsHelper* helper = MakeHelper(web_contents.get());

  std::map<Type, std::vector<std::string>> id_strs;

  // Up to `kMaxIdsPerWebContents` allowed on each type.
  for (Type type : kAllTypes) {
    std::vector<std::string>& ids = id_strs[type];
    for (size_t i = 0; i < kMaxIdsPerWebContents; ++i) {
      ids.push_back(helper->ProduceId(type));
      EXPECT_THAT(ids.back(), IsValidId());
    }
  }

  // Attempts to produce more IDs of either type fail.
  for (Type type : kAllTypes) {
    EXPECT_THAT(helper->ProduceId(type), IsEmptyId());
  }

  // The original IDs are not forgotten.
  for (Type type : kAllTypes) {
    for (const std::string& id_str : id_strs[type]) {
      const base::Token id = GUIDToToken(base::Uuid::ParseLowercase(id_str));
      EXPECT_TRUE(helper->IsAssociatedWith(id, type));
    }
  }
}

TEST_P(SubCaptureTargetIdWebContentsHelperTest,
       CrossDocumentNavigationClearsIdAssociation) {
  std::unique_ptr<TestWebContents> web_contents = MakeTestWebContents();
  SubCaptureTargetIdWebContentsHelper::CreateForWebContents(web_contents.get());
  auto* helper =
      SubCaptureTargetIdWebContentsHelper::FromWebContents(web_contents.get());
  ASSERT_NE(helper, nullptr);

  // Setup - WebContents navigated to a document, ID produced.
  web_contents->NavigateAndCommit(GURL("https://tests-r-us.com/first.html"));
  base::Token id_1;
  {
    const std::string id_str = helper->ProduceId(type_);
    ASSERT_THAT(id_str, IsValidId());
    id_1 = GUIDToToken(base::Uuid::ParseLowercase(id_str));
  }
  ASSERT_TRUE(helper->IsAssociatedWith(id_1, type_));  // Sanity-check.

  // Cross-document navigation occurs.
  web_contents->NavigateAndCommit(GURL("https://tests-r-us.com/second.html"));

  // Verification #1: The old ID is forgotten.
  ASSERT_FALSE(helper->IsAssociatedWith(id_1, type_));

  // Verification #2: New IDs may be recorded.
  {
    const std::string id_str = helper->ProduceId(type_);
    EXPECT_THAT(id_str, IsValidId());
    const base::Token id_2 = GUIDToToken(base::Uuid::ParseLowercase(id_str));
    ASSERT_TRUE(helper->IsAssociatedWith(id_2, type_));  // Sanity-check.
  }

  // Verification #3: The forgotten ID is not counted against the limit
  // of IDs applied to a WebContents. (kMaxIdsPerWebContents - 1 more
  // invocations allowed, then the next one fails.)
  for (size_t i = 0; i < kMaxIdsPerWebContents - 1; ++i) {
    EXPECT_THAT(helper->ProduceId(type_), IsValidId());
  }
  EXPECT_THAT(helper->ProduceId(type_), IsEmptyId());
}

TEST_P(SubCaptureTargetIdWebContentsHelperTest,
       InDocumentNavigationDoesNotClearIdAssociation) {
  std::unique_ptr<TestWebContents> web_contents = MakeTestWebContents();
  SubCaptureTargetIdWebContentsHelper::CreateForWebContents(web_contents.get());
  auto* helper =
      SubCaptureTargetIdWebContentsHelper::FromWebContents(web_contents.get());
  ASSERT_NE(helper, nullptr);

  // Setup - WebContents navigated to a document, ID produced.
  web_contents->NavigateAndCommit(GURL("https://tests-r-us.com/first.html"));
  const std::string id_str = helper->ProduceId(type_);
  ASSERT_THAT(id_str, IsValidId());
  const base::Token id = GUIDToToken(base::Uuid::ParseLowercase(id_str));

  // Test sanity-check.
  ASSERT_TRUE(helper->IsAssociatedWith(id, type_));

  // In-document navigation occurs. The ID is not forgotten.
  web_contents->NavigateAndCommit(GURL("https://tests-r-us.com/first.html#a"));
  EXPECT_TRUE(helper->IsAssociatedWith(id, type_));
}

}  // namespace content
