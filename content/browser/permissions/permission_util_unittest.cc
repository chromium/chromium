// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/permissions/permission_util.h"

#include "base/test/scoped_feature_list.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
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

}  // namespace content
