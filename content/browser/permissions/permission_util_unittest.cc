// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/permissions/permission_util.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-forward.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class PermissionUtilTest : public ::testing::Test {};

TEST_F(PermissionUtilTest, TestIsDomainOverrideNoExtension) {
  blink::mojom::PermissionDescriptorPtr ptr =
      blink::mojom::PermissionDescriptor::New();
  EXPECT_FALSE(PermissionUtil::IsDomainOverride(ptr));
}

TEST_F(PermissionUtilTest, TestIsDomainOverrideWrongExtension) {
  blink::mojom::PermissionDescriptorPtr ptr =
      blink::mojom::PermissionDescriptor::New();
  ptr->extension =
      blink::mojom::PermissionDescriptorExtension::NewMidi(nullptr);
  EXPECT_FALSE(PermissionUtil::IsDomainOverride(ptr));
}

TEST_F(PermissionUtilTest, TestIsDomainOverride) {
  blink::mojom::PermissionDescriptorPtr ptr =
      blink::mojom::PermissionDescriptor::New();
  ptr->extension =
      blink::mojom::PermissionDescriptorExtension::NewTopLevelStorageAccess(
          nullptr);
  EXPECT_TRUE(PermissionUtil::IsDomainOverride(ptr));
}

TEST_F(PermissionUtilTest, TestExtractDomainOverride) {
  url::Origin expected = url::Origin::Create(GURL("https://example.xyz"));
  blink::mojom::PermissionDescriptorPtr ptr =
      blink::mojom::PermissionDescriptor::New();
  auto top_level_storage_access_extension =
      blink::mojom::TopLevelStorageAccessPermissionDescriptor::New();
  top_level_storage_access_extension->requestedOrigin = expected;
  ptr->extension =
      blink::mojom::PermissionDescriptorExtension::NewTopLevelStorageAccess(
          std::move(top_level_storage_access_extension));
  EXPECT_EQ(PermissionUtil::ExtractDomainOverride(ptr), expected);
}

TEST_F(PermissionUtilTest, TestInvalidDomainOverrideFeatureDisabled) {
  EXPECT_FALSE(PermissionUtil::ValidateDomainOverride(
      {}, nullptr, blink::mojom::PermissionDescriptor::New()));
}
TEST_F(PermissionUtilTest, TestInvalidDomainOverrideMultiRequest) {
  std::vector<blink::mojom::PermissionDescriptorPtr> descriptors;
  descriptors.emplace_back(
      PermissionDescriptorUtil::CreatePermissionDescriptorForPermissionType(
          blink::PermissionType::STORAGE_ACCESS_GRANT));
  descriptors.emplace_back(descriptors.front()->Clone());

  EXPECT_FALSE(PermissionUtil::ValidateDomainOverride(
      descriptors, nullptr, blink::mojom::PermissionDescriptor::New()));
}
TEST_F(PermissionUtilTest, TestInvalidDomainOverrideNullRfh) {
  content::BrowserTaskEnvironment task_environment;

  TestBrowserContext browser_context;
  RenderViewHostTestEnabler enabler;
  std::unique_ptr<WebContents> web_contents(
      WebContentsTester::CreateTestWebContents(
          WebContents::CreateParams(&browser_context)));

  WebContentsTester* web_contents_tester =
      WebContentsTester::For(web_contents.get());
  web_contents_tester->NavigateAndCommit(GURL("https://example.xyz"));

  std::vector<blink::mojom::PermissionDescriptorPtr> descriptors;
  descriptors.emplace_back(
      PermissionDescriptorUtil::CreatePermissionDescriptorForPermissionType(
          blink::PermissionType::STORAGE_ACCESS_GRANT));

  EXPECT_FALSE(PermissionUtil::ValidateDomainOverride(
      descriptors, nullptr, blink::mojom::PermissionDescriptor::New()));
}

TEST_F(PermissionUtilTest, TestValidDomainOverride) {
  content::BrowserTaskEnvironment task_environment;

  TestBrowserContext browser_context;
  RenderViewHostTestEnabler enabler;
  std::unique_ptr<WebContents> web_contents(
      WebContentsTester::CreateTestWebContents(
          WebContents::CreateParams(&browser_context)));

  WebContentsTester* web_contents_tester =
      WebContentsTester::For(web_contents.get());
  web_contents_tester->NavigateAndCommit(GURL("https://example.xyz"));

  url::Origin expected =
      url::Origin::Create(GURL("https://another_example.xyz"));
  blink::mojom::PermissionDescriptorPtr descriptor =
      blink::mojom::PermissionDescriptor::New();
  auto top_level_storage_access_extension =
      blink::mojom::TopLevelStorageAccessPermissionDescriptor::New();
  top_level_storage_access_extension->requestedOrigin = expected;
  descriptor->extension =
      blink::mojom::PermissionDescriptorExtension::NewTopLevelStorageAccess(
          std::move(top_level_storage_access_extension));

  std::vector<blink::mojom::PermissionDescriptorPtr> descriptors;
  descriptors.emplace_back(
      PermissionDescriptorUtil::CreatePermissionDescriptorForPermissionType(
          blink::PermissionType::STORAGE_ACCESS_GRANT));

  EXPECT_TRUE(PermissionUtil::ValidateDomainOverride(
      descriptors, web_contents->GetPrimaryMainFrame(), descriptor));
}

TEST_F(PermissionUtilTest, TestSameOriginInvalidDomainOverride) {
  content::BrowserTaskEnvironment task_environment;

  TestBrowserContext browser_context;
  RenderViewHostTestEnabler enabler;
  std::unique_ptr<WebContents> web_contents(
      WebContentsTester::CreateTestWebContents(
          WebContents::CreateParams(&browser_context)));

  WebContentsTester* web_contents_tester =
      WebContentsTester::For(web_contents.get());
  web_contents_tester->NavigateAndCommit(GURL("https://example.xyz"));

  url::Origin expected = url::Origin::Create(GURL("https://example.xyz"));
  blink::mojom::PermissionDescriptorPtr descriptor =
      blink::mojom::PermissionDescriptor::New();
  auto top_level_storage_access_extension =
      blink::mojom::TopLevelStorageAccessPermissionDescriptor::New();
  top_level_storage_access_extension->requestedOrigin = expected;
  descriptor->extension =
      blink::mojom::PermissionDescriptorExtension::NewTopLevelStorageAccess(
          std::move(top_level_storage_access_extension));

  std::vector<blink::mojom::PermissionDescriptorPtr> descriptors;
  descriptors.emplace_back(
      PermissionDescriptorUtil::CreatePermissionDescriptorForPermissionType(
          blink::PermissionType::STORAGE_ACCESS_GRANT));
  EXPECT_FALSE(PermissionUtil::ValidateDomainOverride(
      descriptors, web_contents->GetPrimaryMainFrame(), descriptor));
}

TEST_F(PermissionUtilTest, ToPermissionStatusWithDetails_ContentSetting) {
  PermissionResult result(blink::mojom::PermissionStatus::GRANTED,
                          PermissionStatusSource::UNSPECIFIED,
                          CONTENT_SETTING_ALLOW);
  auto status_with_details = PermissionUtil::ToPermissionStatusWithDetails(
      blink::mojom::PermissionName::GEOLOCATION, result);
  EXPECT_EQ(status_with_details,
            blink::mojom::PermissionStatusWithDetails::New(
                blink::mojom::PermissionStatus::GRANTED, nullptr));
}

TEST_F(PermissionUtilTest,
       ToPermissionStatusWithDetails_GeolocationSetting_Precise) {
  GeolocationSetting geolocation_setting = {
      .approximate = PermissionOption::kAllowed,
      .precise = PermissionOption::kAllowed};
  PermissionResult result(blink::mojom::PermissionStatus::GRANTED,
                          PermissionStatusSource::UNSPECIFIED,
                          geolocation_setting);
  EXPECT_EQ(PermissionUtil::ToPermissionStatusWithDetails(
                blink::mojom::PermissionName::GEOLOCATION, result),
            blink::mojom::PermissionStatusWithDetails::New(
                blink::mojom::PermissionStatus::GRANTED,
                blink::mojom::PermissionDetails::NewGeolocationAccuracy(
                    blink::mojom::GeolocationAccuracy::kPrecise)));
  EXPECT_EQ(PermissionUtil::ToPermissionStatusWithDetails(
                blink::mojom::PermissionName::GEOLOCATION_APPROXIMATE, result),
            blink::mojom::PermissionStatusWithDetails::New(
                blink::mojom::PermissionStatus::GRANTED, nullptr));
}

TEST_F(PermissionUtilTest,
       ToPermissionStatusWithDetails_GeolocationSetting_PreciseDenied) {
  GeolocationSetting geolocation_setting = {
      .approximate = PermissionOption::kAllowed,
      .precise = PermissionOption::kDenied};
  PermissionResult result(blink::mojom::PermissionStatus::GRANTED,
                          PermissionStatusSource::UNSPECIFIED,
                          geolocation_setting);
  EXPECT_EQ(PermissionUtil::ToPermissionStatusWithDetails(
                blink::mojom::PermissionName::GEOLOCATION, result),
            blink::mojom::PermissionStatusWithDetails::New(
                blink::mojom::PermissionStatus::GRANTED,
                blink::mojom::PermissionDetails::NewGeolocationAccuracy(
                    blink::mojom::GeolocationAccuracy::kApproximate)));
  EXPECT_EQ(PermissionUtil::ToPermissionStatusWithDetails(
                blink::mojom::PermissionName::GEOLOCATION_APPROXIMATE, result),
            blink::mojom::PermissionStatusWithDetails::New(
                blink::mojom::PermissionStatus::GRANTED, nullptr));
}

TEST_F(PermissionUtilTest,
       ToPermissionStatusWithDetails_GeolocationSetting_PreciseAsk) {
  GeolocationSetting geolocation_setting = {
      .approximate = PermissionOption::kAllowed,
      .precise = PermissionOption::kAsk};
  PermissionResult precise_result(blink::mojom::PermissionStatus::ASK,
                                  PermissionStatusSource::UNSPECIFIED,
                                  geolocation_setting);
  PermissionResult approximate_result(blink::mojom::PermissionStatus::GRANTED,
                                      PermissionStatusSource::UNSPECIFIED,
                                      geolocation_setting);
  EXPECT_EQ(PermissionUtil::ToPermissionStatusWithDetails(
                blink::mojom::PermissionName::GEOLOCATION, precise_result),
            blink::mojom::PermissionStatusWithDetails::New(
                blink::mojom::PermissionStatus::ASK, nullptr));
  EXPECT_EQ(PermissionUtil::ToPermissionStatusWithDetails(
                blink::mojom::PermissionName::GEOLOCATION_APPROXIMATE,
                approximate_result),
            blink::mojom::PermissionStatusWithDetails::New(
                blink::mojom::PermissionStatus::GRANTED, nullptr));
}

TEST_F(PermissionUtilTest,
       ToPermissionStatusWithDetails_GeolocationSetting_DeniedStatus) {
  GeolocationSetting geolocation_setting = {
      .approximate = PermissionOption::kDenied,
      .precise = PermissionOption::kDenied};
  PermissionResult result(blink::mojom::PermissionStatus::DENIED,
                          PermissionStatusSource::UNSPECIFIED,
                          geolocation_setting);
  EXPECT_EQ(PermissionUtil::ToPermissionStatusWithDetails(
                blink::mojom::PermissionName::GEOLOCATION, result),
            blink::mojom::PermissionStatusWithDetails::New(
                blink::mojom::PermissionStatus::DENIED, nullptr));
  EXPECT_EQ(PermissionUtil::ToPermissionStatusWithDetails(
                blink::mojom::PermissionName::GEOLOCATION_APPROXIMATE, result),
            blink::mojom::PermissionStatusWithDetails::New(
                blink::mojom::PermissionStatus::DENIED, nullptr));
}

TEST_F(PermissionUtilTest,
       ToPermissionStatusWithDetails_GeolocationSetting_AskStatus) {
  GeolocationSetting geolocation_setting = {
      .approximate = PermissionOption::kAsk, .precise = PermissionOption::kAsk};
  PermissionResult result(blink::mojom::PermissionStatus::ASK,
                          PermissionStatusSource::UNSPECIFIED,
                          geolocation_setting);
  EXPECT_EQ(PermissionUtil::ToPermissionStatusWithDetails(
                blink::mojom::PermissionName::GEOLOCATION, result),
            blink::mojom::PermissionStatusWithDetails::New(
                blink::mojom::PermissionStatus::ASK, nullptr));
  EXPECT_EQ(PermissionUtil::ToPermissionStatusWithDetails(
                blink::mojom::PermissionName::GEOLOCATION_APPROXIMATE, result),
            blink::mojom::PermissionStatusWithDetails::New(
                blink::mojom::PermissionStatus::ASK, nullptr));
}

class PermissionUtilGetLastCommittedOriginAsURLTest
    : public RenderViewHostTestHarness {
 protected:
  // Navigates the primary main frame of `web_contents()` to `url`, served with
  // a `Content-Security-Policy: sandbox` header, so that the committed document
  // has an opaque origin whose precursor is the origin of `url`.
  RenderFrameHost* NavigateAndCommitSandboxed(const GURL& url) {
    auto navigation =
        NavigationSimulator::CreateBrowserInitiated(url, web_contents());
    navigation->SetResponseHeaders(net::HttpResponseHeaders::TryToCreate(
        "HTTP/1.1 200 OK\nContent-Security-Policy: sandbox\n\n"));
    navigation->Commit();
    return navigation->GetFinalRenderFrameHost();
  }

  RenderFrameHost* AppendChildAndNavigate(RenderFrameHost* parent,
                                          const GURL& url) {
    RenderFrameHost* child =
        RenderFrameHostTester::For(parent)->AppendChild("child");
    RenderFrameHostTester::For(child)->InitializeRenderFrameIfNeeded();
    return NavigationSimulator::NavigateAndCommitFromDocument(url, child);
  }
};

TEST_F(PermissionUtilGetLastCommittedOriginAsURLTest, MainFrame) {
  NavigateAndCommit(GURL("https://example.com/path?query#ref"));

  EXPECT_EQ(GURL("https://example.com/"),
            PermissionUtil::GetLastCommittedOriginAsURL(main_rfh()));
}

TEST_F(PermissionUtilGetLastCommittedOriginAsURLTest, SameOriginSubframe) {
  NavigateAndCommit(GURL("https://example.com/"));
  RenderFrameHost* child =
      AppendChildAndNavigate(main_rfh(), GURL("https://example.com/child"));

  EXPECT_EQ(GURL("https://example.com/"),
            PermissionUtil::GetLastCommittedOriginAsURL(child));
}

TEST_F(PermissionUtilGetLastCommittedOriginAsURLTest, CrossOriginSubframe) {
  NavigateAndCommit(GURL("https://example.com/"));
  RenderFrameHost* child =
      AppendChildAndNavigate(main_rfh(), GURL("https://other.com/child"));

  EXPECT_EQ(GURL("https://other.com/"),
            PermissionUtil::GetLastCommittedOriginAsURL(child));
}

// Opaque subframes never inherit their precursor origin.
TEST_F(PermissionUtilGetLastCommittedOriginAsURLTest, OpaqueSubframe) {
  NavigateAndCommit(GURL("https://example.com/"));
  RenderFrameHost* child =
      AppendChildAndNavigate(main_rfh(), GURL("data:text/html,child"));
  ASSERT_TRUE(child->GetLastCommittedOrigin().opaque());

  EXPECT_EQ(GURL(), PermissionUtil::GetLastCommittedOriginAsURL(child));
}

// A sandboxed primary main frame is mapped to its precursor origin.
TEST_F(PermissionUtilGetLastCommittedOriginAsURLTest, SandboxedMainFrame) {
  RenderFrameHost* rfh =
      NavigateAndCommitSandboxed(GURL("https://example.com/path"));
  ASSERT_TRUE(rfh->GetLastCommittedOrigin().opaque());

  EXPECT_EQ(GURL("https://example.com/"),
            PermissionUtil::GetLastCommittedOriginAsURL(rfh));
}

// A sandboxed primary main frame keeps using its own precursor origin while a
// navigation to another origin is pending.
TEST_F(PermissionUtilGetLastCommittedOriginAsURLTest,
       SandboxedMainFrameWithPendingNavigation) {
  RenderFrameHost* rfh =
      NavigateAndCommitSandboxed(GURL("https://example.com/path"));
  ASSERT_TRUE(rfh->GetLastCommittedOrigin().opaque());

  const GURL pending_url("https://other.com/");
  auto navigation =
      NavigationSimulator::CreateBrowserInitiated(pending_url, web_contents());
  navigation->Start();
  ASSERT_EQ(pending_url, web_contents()->GetVisibleURL());
  ASSERT_EQ(rfh, web_contents()->GetPrimaryMainFrame());

  EXPECT_EQ(GURL("https://example.com/"),
            PermissionUtil::GetLastCommittedOriginAsURL(rfh));
}

// The initial empty document has an opaque origin without a precursor.
TEST_F(PermissionUtilGetLastCommittedOriginAsURLTest, InitialEmptyDocument) {
  ASSERT_TRUE(main_rfh()->GetLastCommittedOrigin().opaque());

  EXPECT_EQ(GURL(), PermissionUtil::GetLastCommittedOriginAsURL(main_rfh()));
}

// Regression test for https://crbug.com/571195939: the visible URL of a pending
// navigation must not be used as the origin of the still-committed opaque
// initial empty document.
TEST_F(PermissionUtilGetLastCommittedOriginAsURLTest,
       InitialEmptyDocumentWithPendingNavigation) {
  RenderFrameHost* rfh = main_rfh();
  ASSERT_TRUE(rfh->GetLastCommittedOrigin().opaque());

  const GURL pending_url("https://example.com/");
  auto navigation =
      NavigationSimulator::CreateBrowserInitiated(pending_url, web_contents());
  navigation->Start();
  ASSERT_EQ(pending_url, web_contents()->GetVisibleURL());
  ASSERT_EQ(rfh, web_contents()->GetPrimaryMainFrame());

  EXPECT_EQ(GURL(), PermissionUtil::GetLastCommittedOriginAsURL(rfh));
}

}  // namespace content
