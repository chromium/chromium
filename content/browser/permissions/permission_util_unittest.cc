// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/permissions/permission_util.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-forward.h"
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
  EXPECT_FALSE(PermissionUtil::ValidateDomainOverride(
      {blink::PermissionType::STORAGE_ACCESS_GRANT,
       blink::PermissionType::STORAGE_ACCESS_GRANT},
      nullptr, blink::mojom::PermissionDescriptor::New()));
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

  EXPECT_FALSE(PermissionUtil::ValidateDomainOverride(
      {blink::PermissionType::STORAGE_ACCESS_GRANT}, nullptr,
      blink::mojom::PermissionDescriptor::New()));
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

  EXPECT_TRUE(PermissionUtil::ValidateDomainOverride(
      {blink::PermissionType::STORAGE_ACCESS_GRANT},
      web_contents->GetPrimaryMainFrame(), descriptor));
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

  EXPECT_FALSE(PermissionUtil::ValidateDomainOverride(
      {blink::PermissionType::STORAGE_ACCESS_GRANT},
      web_contents->GetPrimaryMainFrame(), descriptor));
}
}  // namespace content
