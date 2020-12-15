// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/crw_wk_navigation_handler.h"

#import "base/test/ios/wait_util.h"
#include "base/test/scoped_feature_list.h"
#include "ios/web/common/features.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/test/web_int_test.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {}  // namespace

namespace web {

class CRWKNavigationHandlerIntTest : public WebIntTest {
 protected:
  CRWKNavigationHandlerIntTest() {
    net::test_server::RegisterDefaultHandlers(&server_);
  }

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kUseDefaultUserAgentInWebClient}, {});
    WebIntTest::SetUp();
  }

  FakeWebClient* GetWebClient() override {
    return static_cast<FakeWebClient*>(WebIntTest::GetWebClient());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  net::test_server::EmbeddedTestServer server_;
};

// Tests that reloading a page with a different default User Agent updates the
// item.
TEST_F(CRWKNavigationHandlerIntTest, ReloadWithDifferentUserAgent) {
  if (@available(iOS 13, *)) {
  } else {
    return;
  }

  FakeWebClient* web_client = GetWebClient();
  web_client->SetDefaultUserAgent(UserAgentType::MOBILE);

  ASSERT_TRUE(server_.Start());
  GURL url(server_.GetURL("/echo"));
  ASSERT_TRUE(LoadUrl(url));

  NavigationItem* item = web_state()->GetNavigationManager()->GetVisibleItem();
  EXPECT_EQ(UserAgentType::MOBILE, item->GetUserAgentType());

  web_client->SetDefaultUserAgent(UserAgentType::DESKTOP);

  web_state()->GetNavigationManager()->Reload(ReloadType::NORMAL,
                                              /* check_for_repost = */ true);

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        NavigationItem* item_after_reload =
            web_state()->GetNavigationManager()->GetVisibleItem();
        return item_after_reload->GetUserAgentType() == UserAgentType::DESKTOP;
      }));
}

}  // namespace web
