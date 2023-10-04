// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cookie_deprecation_label/cookie_deprecation_label_manager_impl.h"

#include "base/test/scoped_feature_list.h"
#include "content/browser/cookie_deprecation_label/cookie_deprecation_label_test_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_content_browser_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class CookieDeprecationLabelManagerImplTest : public testing::Test {
 public:
  explicit CookieDeprecationLabelManagerImplTest()
      : label_manager_(&browser_context_) {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kCookieDeprecationFacilitatedTesting,
        {{"label", "label_test"}});
  }

 protected:
  BrowserTaskEnvironment task_environment_;
  TestBrowserContext browser_context_;
  CookieDeprecationLabelManagerImpl label_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(CookieDeprecationLabelManagerImplTest, NotAllowed_NoLabelReturned) {
  MockCookieDeprecationLabelContentBrowserClientBase<TestContentBrowserClient>
      browser_client;
  EXPECT_CALL(browser_client, IsCookieDeprecationLabelAllowed)
      .WillOnce(testing::Return(false));
  ScopedContentBrowserClientSetting setting(&browser_client);

  EXPECT_FALSE(label_manager_.GetValue().has_value());
}

TEST_F(CookieDeprecationLabelManagerImplTest, Allowed_LabelReturned) {
  MockCookieDeprecationLabelContentBrowserClientBase<TestContentBrowserClient>
      browser_client;
  EXPECT_CALL(browser_client, IsCookieDeprecationLabelAllowed)
      .WillOnce(testing::Return(true));
  ScopedContentBrowserClientSetting setting(&browser_client);

  EXPECT_EQ(label_manager_.GetValue(), "label_test");
}

TEST_F(CookieDeprecationLabelManagerImplTest,
       NotAllowedForContext_NoLabelReturned) {
  MockCookieDeprecationLabelContentBrowserClientBase<TestContentBrowserClient>
      browser_client;
  EXPECT_CALL(browser_client, IsCookieDeprecationLabelAllowedForContext)
      .WillOnce(testing::Return(false));
  ScopedContentBrowserClientSetting setting(&browser_client);

  EXPECT_FALSE(
      label_manager_
          .GetValue(
              /*top_frame_origin=*/url::Origin::Create(GURL("https://a.test")),
              /*context_origin=*/url::Origin::Create(GURL("https://b.test")))
          .has_value());
}

TEST_F(CookieDeprecationLabelManagerImplTest, AllowedForContext_LabelReturned) {
  MockCookieDeprecationLabelContentBrowserClientBase<TestContentBrowserClient>
      browser_client;
  EXPECT_CALL(browser_client, IsCookieDeprecationLabelAllowedForContext)
      .WillOnce(testing::Return(true));
  ScopedContentBrowserClientSetting setting(&browser_client);

  EXPECT_EQ(
      label_manager_.GetValue(
          /*top_frame_origin=*/url::Origin::Create(GURL("https://a.test")),
          /*context_origin=*/url::Origin::Create(GURL("https://b.test"))),
      "label_test");
}

}  // namespace
}  // namespace content
