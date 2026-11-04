// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/https_upgrades/model/https_only_mode_upgrade_tab_helper.h"

#import <map>
#import <memory>
#import <string>
#import <vector>

#import "base/functional/bind.h"
#import "base/strings/stringprintf.h"
#import "base/synchronization/lock.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/https_upgrades/model/https_upgrade_service_factory.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/ios_chrome_test_with_web_state.h"
#import "ios/components/security_interstitials/https_only_mode/feature.h"
#import "ios/components/security_interstitials/https_only_mode/https_only_mode_container.h"
#import "ios/components/security_interstitials/https_only_mode/https_upgrade_service.h"
#import "ios/components/security_interstitials/https_only_mode/https_upgrade_test_util.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/referrer.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#import "ios/web/public/web_state.h"
#import "net/base/apple/url_conversions.h"
#import "net/http/http_request_headers.h"
#import "net/http/http_status_code.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "url/gurl.h"

namespace {

enum class HttpsUpgradesTestType {
  // HTTPS-Upgrades is disabled. HTTPS-Only Mode feature is enabled, but the
  // pref is disabled.
  kNone,
  // HTTPS-Only Mode is enabled (both the feature and the UI preference).
  kHttpsOnlyMode,
  // HTTPS-Upgrades is enabled. HTTPS-Only Mode feature is enabled, but the pref
  // is disabled.
  kHttpsUpgrades,
  // Both HTTPS-Only Mode and HTTPS-Upgrades are enabled.
  kBoth
};
}

std::unique_ptr<KeyedService> BuildFakeHttpsUpgradeService(
    ProfileIOS* profile) {
  return std::make_unique<FakeHttpsUpgradeService>();
}

class HttpsOnlyModeUpgradeTabHelperTest
    : public testing::TestWithParam<HttpsUpgradesTestType> {
 protected:
  HttpsOnlyModeUpgradeTabHelperTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        HttpsUpgradeServiceFactory::GetInstance(),
        base::BindRepeating(&BuildFakeHttpsUpgradeService));

    profile_ = std::move(builder).Build();
    web_state_.SetBrowserState(profile_.get());

    switch (GetParam()) {
      case HttpsUpgradesTestType::kNone:
        profile_->GetPrefs()->SetBoolean(prefs::kHttpsOnlyModeEnabled, false);
        scoped_feature_list_.InitWithFeatures(
            /*enabled_features=*/{},
            /*disabled_features=*/
            {security_interstitials::features::kHttpsUpgrades});
        break;

      case HttpsUpgradesTestType::kHttpsOnlyMode:
        profile_->GetPrefs()->SetBoolean(prefs::kHttpsOnlyModeEnabled, true);
        scoped_feature_list_.InitWithFeatures(
            /*enabled_features=*/{},
            /*disabled_features=*/{
                security_interstitials::features::kHttpsUpgrades});
        break;

      case HttpsUpgradesTestType::kHttpsUpgrades:
        profile_->GetPrefs()->SetBoolean(prefs::kHttpsOnlyModeEnabled, false);

        scoped_feature_list_.InitWithFeatures(
            /*enabled_features=*/{security_interstitials::features::
                                      kHttpsUpgrades},
            /*disabled_features=*/{});
        break;

      case HttpsUpgradesTestType::kBoth:
        profile_->GetPrefs()->SetBoolean(prefs::kHttpsOnlyModeEnabled, true);
        scoped_feature_list_.InitWithFeatures(/*enabled_features=*/
                                              {security_interstitials::
                                                   features::kHttpsUpgrades},
                                              /*disabled_features=*/{});
        break;
    }

    HttpsOnlyModeUpgradeTabHelper::CreateForWebState(
        &web_state_, profile_->GetPrefs(),
        HttpsUpgradeServiceFactory::GetForProfile(profile_.get()));
    HttpsOnlyModeContainer::CreateForWebState(&web_state_);
  }

  void TearDown() override {
    ProfileIOS* profile =
        ProfileIOS::FromBrowserState(web_state_.GetBrowserState());
    HttpsUpgradeService* service =
        HttpsUpgradeServiceFactory::GetForProfile(profile);
    service->ClearAllowlist(base::Time(), base::Time::Max());
  }

  // Helper function that calls into WebState::ShouldAllowResponse with the
  // given `url` and `for_main_frame`, waits for the callback with the decision
  // to be called, and returns the decision.
  web::WebStatePolicyDecider::PolicyDecision ShouldAllowResponseUrl(
      const GURL& url,
      bool for_main_frame) {
    NSURLResponse* response =
        [[NSURLResponse alloc] initWithURL:net::NSURLWithGURL(url)
                                  MIMEType:@"text/html"
                     expectedContentLength:0
                          textEncodingName:nil];
    __block bool callback_called = false;
    __block web::WebStatePolicyDecider::PolicyDecision policy_decision =
        web::WebStatePolicyDecider::PolicyDecision::Allow();
    auto callback =
        base::BindOnce(^(web::WebStatePolicyDecider::PolicyDecision decision) {
          policy_decision = decision;
          callback_called = true;
        });
    web::WebStatePolicyDecider::ResponseInfo response_info(for_main_frame);
    web_state_.ShouldAllowResponse(response, response_info,
                                   std::move(callback));
    EXPECT_TRUE(callback_called);
    return policy_decision;
  }

  // Helper function that calls into WebState::ShouldAllowRequest with the
  // given `url` and `for_main_frame`, waits for the callback with the decision
  // to be called, and returns the decision.
  web::WebStatePolicyDecider::PolicyDecision ShouldAllowRequestUrl(
      const GURL& url,
      bool for_main_frame) {
    NSURLRequest* request =
        [NSURLRequest requestWithURL:net::NSURLWithGURL(url)];
    __block bool callback_called = false;
    __block web::WebStatePolicyDecider::PolicyDecision policy_decision =
        web::WebStatePolicyDecider::PolicyDecision::Allow();
    auto callback =
        base::BindOnce(^(web::WebStatePolicyDecider::PolicyDecision decision) {
          policy_decision = decision;
          callback_called = true;
        });
    web::WebStatePolicyDecider::RequestInfo request_info(
        ui::PAGE_TRANSITION_CLIENT_REDIRECT, for_main_frame,
        /*target_frame_is_cross_origin=*/false,
        /*target_window_is_cross_origin=*/false,
        /*is_user_initiated=*/false, /*user_tapped_recently=*/false);
    web_state_.ShouldAllowRequest(request, request_info, std::move(callback));
    EXPECT_TRUE(callback_called);
    return policy_decision;
  }

  base::HistogramTester histogram_tester_;
  web::FakeWebState web_state_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<ProfileIOS> profile_;
};

// Tests that ShouldAllowResponse properly upgrades navigations and
// ignores subframe navigations. Also tests that UMA records correctly.
TEST_P(HttpsOnlyModeUpgradeTabHelperTest, ShouldAllowResponse) {
  // Create a navigation item.
  auto fake_navigation_manager_ =
      std::make_unique<web::FakeNavigationManager>();
  std::unique_ptr<web::NavigationItem> pending_item =
      web::NavigationItem::Create();
  fake_navigation_manager_->SetPendingItem(pending_item.release());
  web_state_.SetNavigationManager(std::move(fake_navigation_manager_));

  // Main frame HTTPS navigations should be allowed.
  GURL https_url("https://example.com/");
  EXPECT_TRUE(ShouldAllowResponseUrl(https_url, /*main_frame=*/true)
                  .ShouldAllowNavigation());
  // Non-main frame HTTPS navigations should be allowed.
  EXPECT_TRUE(ShouldAllowResponseUrl(https_url, /*main_frame=*/false)
                  .ShouldAllowNavigation());

  // If either HTTPS-Only Mode or HTTPS-Upgrades is enabled, m
  // main frame HTTP navigations should be disallowed.
  GURL http_url("http://example.com/");
  if (GetParam() != HttpsUpgradesTestType::kNone) {
    EXPECT_FALSE(ShouldAllowResponseUrl(http_url, /*main_frame=*/true)
                     .ShouldAllowNavigation());
  } else {
    EXPECT_TRUE(ShouldAllowResponseUrl(http_url, /*main_frame=*/true)
                    .ShouldAllowNavigation());
  }
  // Non-main frame HTTP navigations should be allowed.
  EXPECT_TRUE(ShouldAllowResponseUrl(http_url, /*main_frame=*/false)
                  .ShouldAllowNavigation());

  // Allowlisted hosts shouldn't be blocked.
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_.GetBrowserState());
  HttpsUpgradeService* service =
      HttpsUpgradeServiceFactory::GetForProfile(profile);
  service->AllowHttpForHost("example.com");
  EXPECT_TRUE(ShouldAllowResponseUrl(http_url, /*main_frame=*/true)
                  .ShouldAllowNavigation());
}

// Tests that ShouldAllowRequest allows the initial request of a navigation so
// that ShouldAllowResponse can handle the upgrade, but cancels HTTP redirect
// targets in the middle of an in-progress main frame navigation so that the
// redirect chain does not transit cleartext.
TEST_P(HttpsOnlyModeUpgradeTabHelperTest, ShouldAllowRequest) {
  auto fake_navigation_manager = std::make_unique<web::FakeNavigationManager>();
  std::unique_ptr<web::NavigationItem> pending_item =
      web::NavigationItem::Create();
  fake_navigation_manager->SetPendingItem(pending_item.release());
  web_state_.SetNavigationManager(std::move(fake_navigation_manager));

  GURL https_url("https://example.com/");
  GURL http_url("http://example.com/");

  // Before the navigation has started, the initial request should be allowed
  // regardless of scheme; ShouldAllowResponse handles upgrades for the first
  // hop.
  EXPECT_TRUE(ShouldAllowRequestUrl(http_url, /*main_frame=*/true)
                  .ShouldAllowNavigation());
  EXPECT_TRUE(ShouldAllowRequestUrl(https_url, /*main_frame=*/true)
                  .ShouldAllowNavigation());

  // Start an HTTPS main frame navigation. Subsequent ShouldAllowRequest calls
  // simulate server redirects.
  web::FakeNavigationContext context;
  context.SetUrl(https_url);
  web_state_.OnNavigationStarted(&context);

  // Redirects to HTTPS should always be allowed.
  EXPECT_TRUE(ShouldAllowRequestUrl(https_url, /*main_frame=*/true)
                  .ShouldAllowNavigation());
  // Subframe HTTP requests should be allowed.
  EXPECT_TRUE(ShouldAllowRequestUrl(http_url, /*main_frame=*/false)
                  .ShouldAllowNavigation());
  // Allowlisted hosts shouldn't be blocked.
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_.GetBrowserState());
  HttpsUpgradeService* service =
      HttpsUpgradeServiceFactory::GetForProfile(profile);
  service->AllowHttpForHost("allowlisted.com");
  EXPECT_TRUE(ShouldAllowRequestUrl(GURL("http://allowlisted.com/"),
                                    /*main_frame=*/true)
                  .ShouldAllowNavigation());

  // If either HTTPS-Only Mode or HTTPS-Upgrades is enabled, redirects to HTTP
  // in the main frame should be cancelled before the request is sent.
  if (GetParam() != HttpsUpgradesTestType::kNone) {
    EXPECT_FALSE(ShouldAllowRequestUrl(http_url, /*main_frame=*/true)
                     .ShouldAllowNavigation());
  } else {
    EXPECT_TRUE(ShouldAllowRequestUrl(http_url, /*main_frame=*/true)
                    .ShouldAllowNavigation());
  }
}

// Tests that when a POST navigation redirects to HTTP, ShouldAllowRequest
// cancels the HTTP redirect when HTTPS-Only Mode or HTTPS-Upgrades is enabled.
TEST_P(HttpsOnlyModeUpgradeTabHelperTest,
       ShouldAllowRequestPostRedirectToHttp) {
  auto fake_navigation_manager = std::make_unique<web::FakeNavigationManager>();
  std::unique_ptr<web::NavigationItem> pending_item =
      web::NavigationItem::Create();
  fake_navigation_manager->SetPendingItem(pending_item.release());
  web_state_.SetNavigationManager(std::move(fake_navigation_manager));

  GURL https_url("https://example.com/");
  GURL http_url("http://example.com/");

  // Start an HTTPS POST navigation.
  web::FakeNavigationContext context;
  context.SetUrl(https_url);
  context.SetIsPost(true);
  web_state_.OnNavigationStarted(&context);

  // If either HTTPS-Only Mode or HTTPS-Upgrades is enabled, redirects to HTTP
  // in the main frame should be cancelled before the request is sent.
  if (GetParam() != HttpsUpgradesTestType::kNone) {
    EXPECT_FALSE(ShouldAllowRequestUrl(http_url, /*main_frame=*/true)
                     .ShouldAllowNavigation());
  } else {
    EXPECT_TRUE(ShouldAllowRequestUrl(http_url, /*main_frame=*/true)
                    .ShouldAllowNavigation());
  }
}

TEST_P(HttpsOnlyModeUpgradeTabHelperTest, GetUpgradedHttpsUrl) {
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_.GetBrowserState());
  HttpsUpgradeService* service =
      HttpsUpgradeServiceFactory::GetForProfile(profile);

  service->SetHttpsPortForTesting(/*https_port_for_testing=*/0,
                                  /*use_fake_https_for_testing=*/false);
  EXPECT_EQ(GURL("https://example.com/test"),
            service->GetUpgradedHttpsUrl(GURL("http://example.com/test")));
  // use_fake_https_for_testing=true with https_port_for_testing=0 is not
  // supported.

  service->SetHttpsPortForTesting(/*https_port_for_testing=*/0,
                                  /*use_fake_https_for_testing=*/false);
  EXPECT_EQ(GURL("https://example.com:8000/test"),
            service->GetUpgradedHttpsUrl(GURL("http://example.com:8000/test")));
  // use_fake_https_for_testing=true with https_port_for_testing=0 is not
  // supported.

  service->SetHttpsPortForTesting(/*https_port_for_testing=*/8001,
                                  /*use_fake_https_for_testing=*/false);
  EXPECT_EQ(GURL("https://example.com:8001/test"),
            service->GetUpgradedHttpsUrl(GURL("http://example.com:8000/test")));

  service->SetHttpsPortForTesting(/*https_port_for_testing=*/8001,
                                  /*use_fake_https_for_testing=*/true);
  EXPECT_EQ(GURL("http://example.com:8001/test#fake-https"),
            service->GetUpgradedHttpsUrl(GURL("http://example.com:8000/test")));
}

INSTANTIATE_TEST_SUITE_P(
    /* No InstantiationName */,
    HttpsOnlyModeUpgradeTabHelperTest,
    testing::Values(HttpsUpgradesTestType::kNone,
                    HttpsUpgradesTestType::kHttpsOnlyMode,
                    HttpsUpgradesTestType::kHttpsUpgrades,
                    HttpsUpgradesTestType::kBoth));

namespace {

using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

constexpr char kFirstPagePath[] = "/first_page.html";
constexpr char kSecondPagePath[] = "/second_page.html";
constexpr char kFallbackPagePath[] = "/defaultresponse";
constexpr char kFaviconPath[] = "/favicon.ico";

constexpr char kSecondPageLinkId[] = "second-page-link";
constexpr char kNoReferrerFallbackLinkId[] = "no-referrer-fallback-link";

constexpr char kLocationReloadScript[] = "location.reload();";
constexpr base::TimeDelta kFallbackDelay = base::Milliseconds(20);

// Thread-safe recorder of HTTP requests and their `Referer` headers.
class RequestRecorder {
 public:
  RequestRecorder() = default;
  ~RequestRecorder() = default;

  void Record(const net::test_server::HttpRequest& request) {
    if (request.relative_url == kFaviconPath) {
      return;
    }
    base::AutoLock auto_lock(lock_);
    auto it = request.headers.find(net::HttpRequestHeaders::kReferer);
    std::string referer = (it != request.headers.end()) ? it->second : "";
    referrers_by_path_[request.relative_url].push_back(referer);
  }

  void Clear() {
    base::AutoLock auto_lock(lock_);
    referrers_by_path_.clear();
  }

  std::vector<std::string> GetReferrers(const std::string& path) {
    base::AutoLock auto_lock(lock_);
    auto it = referrers_by_path_.find(path);
    return it == referrers_by_path_.end() ? std::vector<std::string>()
                                          : it->second;
  }

 private:
  base::Lock lock_;
  std::map<std::string, std::vector<std::string>> referrers_by_path_;
};

}  // namespace

// Integration test fixture using a real `WKWebView`-backed `WebState` and
// embedded HTTP / faux-HTTPS test servers to verify whether stale
// `NavigationItem` referrers reach upgraded and fallback requests in
// `HttpsOnlyModeUpgradeTabHelper`.
class HttpsOnlyModeUpgradeTabHelperReferrerTest
    : public IOSChromeTestWithWebState {
 protected:
  void SetUp() override {
    IOSChromeTestWithWebState::SetUp();

    scoped_feature_list_.InitAndEnableFeature(
        security_interstitials::features::kHttpsUpgrades);

    ProfileIOS* profile =
        ProfileIOS::FromBrowserState(web_state()->GetBrowserState());
    profile->GetPrefs()->SetBoolean(prefs::kHttpsOnlyModeEnabled, false);

    http_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::test_server::EmbeddedTestServer::TYPE_HTTP);
    net::test_server::RegisterDefaultHandlers(http_server_.get());
    http_server_->RegisterRequestMonitor(base::BindRepeating(
        &RequestRecorder::Record, base::Unretained(&http_recorder_)));
    ASSERT_TRUE(http_server_->Start());

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::test_server::EmbeddedTestServer::TYPE_HTTP);
    https_server_->RegisterRequestMonitor(base::BindRepeating(
        &RequestRecorder::Record, base::Unretained(&https_recorder_)));
    https_server_->RegisterRequestHandler(base::BindRepeating(
        &HttpsOnlyModeUpgradeTabHelperReferrerTest::HandleHttpsRequest,
        base::Unretained(this)));
    net::test_server::RegisterDefaultHandlers(https_server_.get());
    ASSERT_TRUE(https_server_->Start());

    HttpsUpgradeService* service =
        HttpsUpgradeServiceFactory::GetForProfile(profile);
    service->SetHttpsPortForTesting(https_server_->port(),
                                    /*use_fake_https_for_testing=*/true);
    service->SetFallbackHttpPortForTesting(http_server_->port());
    service->SetFallbackDelayForTesting(kFallbackDelay);

    web_state()->GetView();
    web_state()->SetKeepRenderProcessAlive(true);
    HttpsOnlyModeContainer::CreateForWebState(web_state());
    HttpsOnlyModeUpgradeTabHelper::CreateForWebState(
        web_state(), profile->GetPrefs(), service);
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleHttpsRequest(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url == kFirstPagePath) {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content_type("text/html");
      response->set_content(base::StringPrintf(
          "<html><body><a id=\"%s\" href=\"%s\">Second Page</a>"
          "</body></html>",
          kSecondPageLinkId, kSecondPagePath));
      return response;
    }
    if (request.relative_url == kSecondPagePath) {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content_type("text/html");
      response->set_content(base::StringPrintf(
          "<html><body>"
          "<a id=\"%s\" rel=\"noreferrer\" "
          "href=\"%s\">Fallback Page</a>"
          "</body></html>",
          kNoReferrerFallbackLinkId,
          http_server_->GetURL(kFallbackPagePath).spec().c_str()));
      return response;
    }
    if (request.relative_url == kFallbackPagePath) {
      // `GetUpgradedHttpsUrl()` preserves the path from `http_server_`, so hang
      // `kFallbackPagePath` on `https_server_` to trigger `FallbackToHttp()`
      // while `RegisterDefaultHandlers` serves 200 OK on `http_server_`.
      return std::make_unique<net::test_server::HungResponse>();
    }
    return nullptr;
  }

  // Navigates to `kFirstPagePath` and clicks the link to `kSecondPagePath` so
  // `kSecondPagePath` commits with `kFirstPagePath` as its `Referer`.
  void NavigateToSecondPage() {
    const GURL first_url = https_server_->GetURL(kFirstPagePath);
    const GURL second_url = https_server_->GetURL(kSecondPagePath);

    ASSERT_TRUE(LoadUrl(first_url));

    ASSERT_TRUE(
        web::test::TapWebViewElementWithId(web_state(), kSecondPageLinkId));
    ASSERT_TRUE(WaitUntilConditionOrTimeout(
        kWaitForPageLoadTimeout,
        /*run_message_loop=*/true, ^{
          web::NavigationItem* item =
              web_state()->GetNavigationManager()->GetLastCommittedItem();
          return !web_state()->IsLoading() && item &&
                 item->GetURL() == second_url;
        }));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  RequestRecorder http_recorder_;
  RequestRecorder https_recorder_;
  std::unique_ptr<net::EmbeddedTestServer> http_server_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

// Tests that `HttpsOnlyModeUpgradeTabHelper` does not send a prior navigation's
// `Referer` header over the wire on upgraded HTTPS or HTTP fallback requests
// when reloading a page reached via `rel="noreferrer"`.
TEST_F(HttpsOnlyModeUpgradeTabHelperReferrerTest,
       StaleReferrerOnUpgradeAndFallbackRequests) {
  const GURL first_url = https_server_->GetURL(kFirstPagePath);
  const GURL http_fallback_url = http_server_->GetURL(kFallbackPagePath);

  // 1. Navigates to `kFirstPagePath` and clicks the link to `kSecondPagePath`
  // (`Referer` is `kFirstPagePath`).
  NavigateToSecondPage();
  web::NavigationItem* second_item =
      web_state()->GetNavigationManager()->GetLastCommittedItem();
  ASSERT_TRUE(second_item);
  EXPECT_EQ(first_url, second_item->GetReferrer().url);

  http_recorder_.Clear();
  https_recorder_.Clear();

  // 2. Clicks `<a rel="noreferrer" href="http://.../defaultresponse">`.
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(),
                                                 kNoReferrerFallbackLinkId));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(
      kWaitForPageLoadTimeout,
      /*run_message_loop=*/true, ^{
        web::NavigationItem* item =
            web_state()->GetNavigationManager()->GetLastCommittedItem();
        return !web_state()->IsLoading() && item &&
               item->GetURL() == http_fallback_url &&
               http_recorder_.GetReferrers(kFallbackPagePath).size() >= 2u &&
               https_recorder_.GetReferrers(kFallbackPagePath).size() >= 1u;
      }));

  std::vector<std::string> initial_http_refs =
      http_recorder_.GetReferrers(kFallbackPagePath);
  std::vector<std::string> initial_https_refs =
      https_recorder_.GetReferrers(kFallbackPagePath);
  ASSERT_EQ(2u, initial_http_refs.size());
  ASSERT_EQ(1u, initial_https_refs.size());
  web::NavigationItem* fallback_item =
      web_state()->GetNavigationManager()->GetLastCommittedItem();
  ASSERT_TRUE(fallback_item);
  EXPECT_EQ("", initial_http_refs[0]);
  EXPECT_EQ("", initial_https_refs[0]);
  EXPECT_EQ("", initial_http_refs[1]);
  EXPECT_EQ(GURL(), fallback_item->GetReferrer().url);

  // 3. Reloads the committed HTTP fallback item and verifies that neither the
  // upgraded HTTPS request nor the fallback HTTP request sends `kFirstPagePath`
  // as `Referer`.
  http_recorder_.Clear();
  https_recorder_.Clear();
  web_state()->GetNavigationManager()->Reload(web::ReloadType::NORMAL,
                                              /*check_for_repost=*/true);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(
      kWaitForPageLoadTimeout,
      /*run_message_loop=*/true, ^{
        return !web_state()->IsLoading() &&
               http_recorder_.GetReferrers(kFallbackPagePath).size() >= 2u &&
               https_recorder_.GetReferrers(kFallbackPagePath).size() >= 1u;
      }));

  std::vector<std::string> reload_http_refs =
      http_recorder_.GetReferrers(kFallbackPagePath);
  std::vector<std::string> reload_https_refs =
      https_recorder_.GetReferrers(kFallbackPagePath);
  ASSERT_EQ(2u, reload_http_refs.size());
  ASSERT_EQ(1u, reload_https_refs.size());
  web::NavigationItem* reloaded_item =
      web_state()->GetNavigationManager()->GetLastCommittedItem();
  ASSERT_TRUE(reloaded_item);
  EXPECT_EQ("", reload_http_refs[0]);
  EXPECT_EQ("", reload_https_refs[0]);
  EXPECT_EQ("", reload_http_refs[1]);
  EXPECT_EQ(GURL(), reloaded_item->GetReferrer().url);
}

// Tests that a page-script-initiated `location.reload()` on a page reached via
// `rel="noreferrer"` does not send a prior navigation's `Referer` header over
// the wire on upgraded HTTPS or HTTP fallback requests.
TEST_F(HttpsOnlyModeUpgradeTabHelperReferrerTest,
       StaleReferrerOnScriptLocationReload) {
  const GURL http_fallback_url = http_server_->GetURL(kFallbackPagePath);

  // 1. Navigates `kFirstPagePath` -> `kSecondPagePath` -> `rel="noreferrer"`
  // `http://.../defaultresponse`.
  NavigateToSecondPage();
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(),
                                                 kNoReferrerFallbackLinkId));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(
      kWaitForPageLoadTimeout,
      /*run_message_loop=*/true, ^{
        web::NavigationItem* item =
            web_state()->GetNavigationManager()->GetLastCommittedItem();
        return !web_state()->IsLoading() && item &&
               item->GetURL() == http_fallback_url &&
               http_recorder_.GetReferrers(kFallbackPagePath).size() >= 2u &&
               https_recorder_.GetReferrers(kFallbackPagePath).size() >= 1u;
      }));
  web::NavigationItem* fallback_item =
      web_state()->GetNavigationManager()->GetLastCommittedItem();
  ASSERT_TRUE(fallback_item);
  EXPECT_EQ(GURL(), fallback_item->GetReferrer().url);

  // 2. Triggers page-initiated `location.reload()` on the fallback item.
  http_recorder_.Clear();
  https_recorder_.Clear();
  web::test::ExecuteJavaScript(web_state(), kLocationReloadScript);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(
      kWaitForPageLoadTimeout,
      /*run_message_loop=*/true, ^{
        return !web_state()->IsLoading() &&
               http_recorder_.GetReferrers(kFallbackPagePath).size() >= 2u &&
               https_recorder_.GetReferrers(kFallbackPagePath).size() >= 1u;
      }));

  std::vector<std::string> js_http_refs =
      http_recorder_.GetReferrers(kFallbackPagePath);
  std::vector<std::string> js_https_refs =
      https_recorder_.GetReferrers(kFallbackPagePath);
  ASSERT_EQ(2u, js_http_refs.size());
  ASSERT_EQ(1u, js_https_refs.size());
  web::NavigationItem* js_reloaded_item =
      web_state()->GetNavigationManager()->GetLastCommittedItem();
  ASSERT_TRUE(js_reloaded_item);
  // WebKit's page-initiated `location.reload()` sends the page's own URL as
  // `Referer` on the initial reload request, while the upgraded HTTPS and
  // fallback HTTP loads initiated by `HttpsOnlyModeUpgradeTabHelper` send no
  // `Referer`.
  EXPECT_EQ(http_fallback_url.spec(), js_http_refs[0]);
  EXPECT_EQ("", js_https_refs[0]);
  EXPECT_EQ("", js_http_refs[1]);
  EXPECT_EQ(GURL(), js_reloaded_item->GetReferrer().url);
}
