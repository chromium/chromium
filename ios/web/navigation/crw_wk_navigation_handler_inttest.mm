// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/crw_wk_navigation_handler.h"

#include "base/scoped_observation.h"
#import "base/test/ios/wait_util.h"
#include "base/test/scoped_feature_list.h"
#include "ios/web/common/features.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/navigation_test_util.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/test/web_int_test.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Constant for timeout in seconds while waiting for a failed HTTPS upgrade to
// complete. This is longer than usual timeouts as the failed HTTPS upgrade
// tests take a long time.
const NSTimeInterval kFailedHttpsTimeout = 100.0;

class FailedWebStateObserver : public web::WebStateObserver {
 public:
  FailedWebStateObserver() = default;
  FailedWebStateObserver(const FailedWebStateObserver&) = delete;
  FailedWebStateObserver& operator=(const FailedWebStateObserver&) = delete;

  void DidFinishNavigation(
      web::WebState* web_state,
      web::NavigationContext* navigation_context) override {
    did_finish_ = true;
    failed_https_upgrade_ = navigation_context->IsFailedHTTPSUpgrade();
  }

  void WebStateDestroyed(web::WebState* web_state) override { NOTREACHED(); }

  bool did_finish() const { return did_finish_; }
  bool failed_https_upgrade() const { return failed_https_upgrade_; }

 private:
  bool did_finish_ = false;
  bool failed_https_upgrade_ = false;
};

}  // namespace

namespace web {

class CRWKNavigationHandlerIntTest : public WebIntTest {
 protected:
  CRWKNavigationHandlerIntTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    net::test_server::RegisterDefaultHandlers(&server_);
    net::test_server::RegisterDefaultHandlers(&https_server_);
  }

  FakeWebClient* GetWebClient() override {
    return static_cast<FakeWebClient*>(WebIntTest::GetWebClient());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  net::test_server::EmbeddedTestServer server_;
  net::EmbeddedTestServer https_server_;
};

// Tests that reloading a page with a different default User Agent updates the
// item.
TEST_F(CRWKNavigationHandlerIntTest, ReloadWithDifferentUserAgent) {
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

// Tests that an SSL or net error on a navigation that wasn't upgraded to HTTPS
// doesn't set the IsFailedHTTPSUpgrade() bit on the navigation context.
TEST_F(CRWKNavigationHandlerIntTest, FailedHTTPSUpgrade_NotUpgraded) {
  ASSERT_TRUE(https_server_.Start());
  GURL url(https_server_.GetURL("/echo"));
  web::NavigationManager::WebLoadParams params(url);
  params.transition_type = ui::PAGE_TRANSITION_TYPED;
  params.is_using_https_as_default_scheme = false;

  FailedWebStateObserver observer;
  base::ScopedObservation<WebState, WebStateObserver> scoped_observer(
      &observer);
  scoped_observer.Observe(web_state());
  web_state()->GetNavigationManager()->LoadURLWithParams(params);

  // Need to use a pointer to |observer| as the block wants to capture it by
  // value (even if marked with __block) which would not work.
  FailedWebStateObserver* observer_ptr = &observer;
  EXPECT_TRUE(
      base::test::ios::WaitUntilConditionOrTimeout(kFailedHttpsTimeout, ^{
        return observer_ptr->did_finish() &&
               !observer_ptr->failed_https_upgrade();
      }));
}

// Tests that an SSL or net error on a navigation that was upgraded to HTTPS
// sets the IsFailedHTTPSUpgrade() bit on the navigation context.
TEST_F(CRWKNavigationHandlerIntTest, FailedHTTPSUpgrade_Upgraded) {
  ASSERT_TRUE(https_server_.Start());
  GURL url(https_server_.GetURL("/echo"));
  web::NavigationManager::WebLoadParams params(url);
  params.transition_type = ui::PAGE_TRANSITION_TYPED;
  params.is_using_https_as_default_scheme = true;

  FailedWebStateObserver observer;
  base::ScopedObservation<WebState, WebStateObserver> scoped_observer(
      &observer);
  scoped_observer.Observe(web_state());
  web_state()->GetNavigationManager()->LoadURLWithParams(params);

  // Need to use a pointer to |observer| as the block wants to capture it by
  // value (even if marked with __block) which would not work.
  FailedWebStateObserver* observer_ptr = &observer;
  EXPECT_TRUE(
      base::test::ios::WaitUntilConditionOrTimeout(kFailedHttpsTimeout, ^{
        return observer_ptr->did_finish() &&
               observer_ptr->failed_https_upgrade();
      }));
}

}  // namespace web
