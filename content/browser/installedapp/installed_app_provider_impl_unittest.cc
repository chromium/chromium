// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/installedapp/installed_app_provider_impl.h"

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "content/browser/installedapp/installed_app_provider_impl.h"
#include "content/browser/installedapp/test/installed_app_provider_impl_test_utils.h"
#include "content/common/features.h"
#include "content/public/common/content_features.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/installedapp/related_application.mojom.h"

#if BUILDFLAG(IS_WIN)
#include "content/browser/installedapp/native_win_app_fetcher.h"
#endif  // BUILDFLAG(IS_WIN)

namespace content {

namespace {

using testing::Contains;
using testing::IsEmpty;
using testing::Not;

const char kInstalledWinAppId[] = "a";
const char kInstalledWebAppId[] = "http://foo.com/";

const std::vector<std::string> kInstalledWinAppIds = {
    kInstalledWinAppId, "B", "C", "D", "E", "F", "G"};
const std::vector<std::string> kInstalledWebAppIds = {
    kInstalledWebAppId, "http://foo.com/test", "http://foo2.com",
    "http://bar.com"};
}  // namespace

class InstalledAppProviderImplTest : public RenderViewHostImplTestHarness {
 public:
  explicit InstalledAppProviderImplTest()
      : content_browser_client_(kInstalledWebAppIds) {
    feature_list_.InitWithFeatures(
        {features::kInstalledAppProvider,
#if BUILDFLAG(IS_WIN)
         features::kFilterInstalledAppsWinMatching,
#endif  // BUILDFLAG(IS_WIN)
         features::kFilterInstalledAppsWebAppMatching},
        {});
  }

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    old_content_browser_client_ =
        SetBrowserClientForTesting(&content_browser_client_);

    // Owned ptr, we must be careful to destroy this on TearDown().
    provider_ = InstalledAppProviderImpl::CreateForTesting(
        *(contents()->GetPrimaryMainFrame()),
        remote_.BindNewPipeAndPassReceiver());
#if BUILDFLAG(IS_WIN)
    provider_->SetNativeWinAppFetcherFactoryForTesting(
        base::BindRepeating(&CreateFakeNativeWinAppFetcherForTesting,
                            std::move(kInstalledWinAppIds)));
#endif  // BUILDFLAG(IS_WIN)
  }

  void TearDown() override {
    SetBrowserClientForTesting(old_content_browser_client_);
    // Owned ptr, we must be careful to destroy this on TearDown().
    provider_ = nullptr;
    RenderViewHostImplTestHarness::TearDown();
  }

  mojo::Remote<blink::mojom::InstalledAppProvider>& remote() { return remote_; }

 private:
  base::test::ScopedFeatureList feature_list_;

  mojo::Remote<blink::mojom::InstalledAppProvider> remote_;
  FakeContentBrowserClientForQueryInstalledWebApps content_browser_client_;
  raw_ptr<InstalledAppProviderImpl> provider_;
  raw_ptr<content::ContentBrowserClient> old_content_browser_client_ = nullptr;
};

MATCHER_P(RelatedAppById, app_id, "") {
  if (app_id != arg->id) {
    *result_listener << arg->id.value() << " doesn't match expected " << app_id;
  }
  return arg->id == app_id;
}

TEST_F(InstalledAppProviderImplTest, GetRelatedApps) {
  std::vector<blink::mojom::RelatedApplicationPtr> related_applications;
  const std::string unknown_web_app_id = "http://unknownid.com";

  // Test that related application matching on windows does not rely on
  // capitalization.
  related_applications.push_back(CreateRelatedApplicationFromPlatformAndId(
      "windows", base::ToUpperASCII(kInstalledWinAppId)));
  related_applications.push_back(
      CreateRelatedApplicationFromPlatformAndId("webapp", kInstalledWebAppId));
  related_applications.push_back(
      CreateRelatedApplicationFromPlatformAndId("webapp", unknown_web_app_id));

  base::test::TestFuture<std::vector<blink::mojom::RelatedApplicationPtr>>
      future;

  remote()->FilterInstalledApps(std::move(related_applications),
                                GURL("http://foo.com/manifest.json"),
                                future.GetCallback());

  ASSERT_TRUE(future.Wait());
  const std::vector<blink::mojom::RelatedApplicationPtr>& result = future.Get();

  std::size_t expected_number_of_matches = 0u;
#if BUILDFLAG(IS_WIN)
  expected_number_of_matches += 1u;
#endif  // BUILDFLAG(IS_WIN)
#if !BUILDFLAG(IS_ANDROID)
  expected_number_of_matches += 1u;
#endif  // !BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(result.size(), expected_number_of_matches);

#if BUILDFLAG(IS_WIN)
  EXPECT_THAT(result, Contains(RelatedAppById(kInstalledWinAppId)));
#endif  // BUILDFLAG(IS_WIN)
#if !BUILDFLAG(IS_ANDROID)
  EXPECT_THAT(result, Contains(RelatedAppById(kInstalledWebAppId)));
#endif  // !BUILDFLAG(IS_ANDROID)

  // Should not be in list.
  EXPECT_THAT(result, Not(Contains(RelatedAppById(unknown_web_app_id))));
}

TEST_F(InstalledAppProviderImplTest,
       ShouldReturnNothingWithEmptyRelatedApplications) {
  base::test::TestFuture<std::vector<blink::mojom::RelatedApplicationPtr>>
      future;

  // Empty related apps list
  remote()->FilterInstalledApps(
      std::vector<blink::mojom::RelatedApplicationPtr>(),
      GURL("http://foo.com/manifest.json"), future.GetCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_THAT(future.Get(), IsEmpty());
}

TEST_F(InstalledAppProviderImplTest,
       ShouldNotReturnWebAppIfManifestIdIsInvalid) {
  const std::string invalid_web_app_id = "http:invalid-url";
  std::vector<blink::mojom::RelatedApplicationPtr> related_applications;
  related_applications.push_back(
      CreateRelatedApplicationFromPlatformAndId("webapp", invalid_web_app_id));

  base::test::TestFuture<std::vector<blink::mojom::RelatedApplicationPtr>>
      future;
  remote()->FilterInstalledApps(std::move(related_applications),
                                GURL("http://foo.com/manifest.json"),
                                future.GetCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_THAT(future.Get(), IsEmpty());
}

#if BUILDFLAG(IS_WIN)
TEST_F(InstalledAppProviderImplTest, LimitNumberOfMatchedApps) {
  std::vector<blink::mojom::RelatedApplicationPtr> related_applications;

  for (const std::string& id : kInstalledWebAppIds) {
    related_applications.push_back(
        CreateRelatedApplicationFromPlatformAndId("webapp", id));
  }
  for (const std::string& id : kInstalledWinAppIds) {
    related_applications.push_back(
        CreateRelatedApplicationFromPlatformAndId("windows", id));
  }
  EXPECT_EQ(related_applications.size(), 11u);

  base::test::TestFuture<std::vector<blink::mojom::RelatedApplicationPtr>>
      future;
  remote()->FilterInstalledApps(std::move(related_applications),
                                GURL("http://foo.com/manifest.json"),
                                future.GetCallback());

  ASSERT_TRUE(future.Wait());
  const std::vector<blink::mojom::RelatedApplicationPtr>& result = future.Get();
  EXPECT_EQ(result.size(), 10u);
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace content
