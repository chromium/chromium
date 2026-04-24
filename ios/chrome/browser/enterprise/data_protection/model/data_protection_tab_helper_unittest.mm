// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_protection/model/data_protection_tab_helper.h"

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/strings/stringprintf.h"
#import "base/test/bind.h"
#import "base/test/test_future.h"
#import "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#import "components/enterprise/connectors/core/connectors_prefs.h"
#import "components/enterprise/connectors/core/reporting_constants.h"
#import "components/enterprise/data_controls/core/browser/prefs.h"
#import "components/enterprise/data_controls/core/browser/test_utils.h"
#import "components/enterprise/data_protection/data_protection_url_lookup_service.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "components/safe_browsing/core/browser/realtime/fake_url_lookup_service.h"
#import "components/safe_browsing/core/browser/realtime/url_lookup_service.h"
#import "components/safe_browsing/core/browser/realtime/url_lookup_service_base.h"
#import "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#import "ios/chrome/browser/enterprise/data_controls/model/ios_rules_service.h"
#import "ios/chrome/browser/enterprise/data_controls/model/ios_rules_service_factory.h"
#import "ios/chrome/browser/enterprise/data_protection/model/data_protection_tab_helper_observer.h"
#import "ios/chrome/browser/enterprise/data_protection/model/data_protection_url_lookup_service_factory.h"
#import "ios/chrome/browser/safe_browsing/model/real_time_url_lookup_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#import "services/network/test/test_url_loader_factory.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using ::testing::_;

namespace {

const char kProtectedUrl[] = "https://protected.com";
const char kUnprotectedUrl[] = "https://unprotected.com";
const char kFakeDmToken[] = "fake-dm-token";
const char kFakeClientId[] = "fake-client-id";

// Fake for RealTimeUrlLookupService.
class FakeRealTimeUrlLookupService
    : public safe_browsing::testing::FakeRealTimeUrlLookupService {
 public:
  using safe_browsing::testing::FakeRealTimeUrlLookupService::
      FakeRealTimeUrlLookupService;

  void set_fake_response(
      std::unique_ptr<safe_browsing::RTLookupResponse> response) {
    fake_response_ = std::move(response);
  }

  void set_is_rt_lookup_successful(bool successful) {
    is_rt_lookup_successful_ = successful;
  }

  void RunResponseCallback(
      std::unique_ptr<safe_browsing::RTLookupResponse> response) {
    ASSERT_FALSE(pending_callbacks_.empty());
    std::move(pending_callbacks_.front())
        .Run(is_rt_lookup_successful_, /*is_cached_response=*/false,
             std::move(response));
    pending_callbacks_.pop();
  }

  size_t start_lookup_count() const { return start_lookup_count_; }

  // RealTimeUrlLookupServiceBase:
  void StartLookup(
      const GURL& url,
      safe_browsing::RTLookupResponseCallback response_callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      SessionID tab_id,
      std::optional<safe_browsing::internal::ReferringAppInfo>
          referring_app_info) override {
    NOTREACHED();
  }

  void StartMaybeCachedLookup(
      const GURL& url,
      safe_browsing::RTLookupResponseCallback response_callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      SessionID tab_id,
      std::optional<safe_browsing::internal::ReferringAppInfo>
          referring_app_info,
      bool use_cache) override {
    start_lookup_count_++;
    pending_callbacks_.push(std::move(response_callback));
  }

 private:
  base::queue<safe_browsing::RTLookupResponseCallback> pending_callbacks_;
  std::unique_ptr<safe_browsing::RTLookupResponse> fake_response_;
  bool is_rt_lookup_successful_ = true;
  size_t start_lookup_count_ = 0;
};

// Fake for DataProtectionTabHelperObserver.
class TestDataProtectionTabHelperObserver
    : public DataProtectionTabHelperObserver {
 public:
  TestDataProtectionTabHelperObserver() = default;

  // DataProtectionTabHelperObserver:
  void ScreenshotProtectionDidChange(
      web::WebState* web_state,
      bool screenshot_protection_enabled) override {
    // Record the protection state.
    future_.SetValue(screenshot_protection_enabled);
  }

  // Waits for the notification and returns the reported protection state.
  bool Wait() { return future_.Take(); }

 private:
  base::test::TestFuture<bool> future_;
};

std::unique_ptr<KeyedService> CreateDataProtectionUrlLookupService(
    ProfileIOS* profile) {
  return std::make_unique<
      enterprise_data_protection::DataProtectionUrlLookupService>();
}

}  // namespace

class DataProtectionTabHelperTest : public PlatformTest {
 protected:
  DataProtectionTabHelperTest() {
    fake_dm_token_storage_ =
        std::make_unique<policy::FakeBrowserDMTokenStorage>();
    fake_dm_token_storage_->SetDMToken(kFakeDmToken);
    fake_dm_token_storage_->SetClientId(kFakeClientId);
    policy::BrowserDMTokenStorage::SetForTesting(fake_dm_token_storage_.get());

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        RealTimeUrlLookupServiceFactory::GetInstance(),
        base::BindRepeating(
            &DataProtectionTabHelperTest::CreateFakeRealTimeUrlLookupService,
            base::Unretained(this)));
    builder.AddTestingFactory(
        DataProtectionUrlLookupServiceFactory::GetInstance(),
        base::BindRepeating(&CreateDataProtectionUrlLookupService));

    profile_ = std::move(builder).Build();
    profile_->SetSharedURLLoaderFactory(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_));

    // Enable real-time lookup by default via prefs.
    profile_->GetPrefs()->SetInteger(
        enterprise_connectors::kEnterpriseRealTimeUrlCheckMode,
        enterprise_connectors::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);
    profile_->GetPrefs()->SetInteger(
        enterprise_connectors::kEnterpriseRealTimeUrlCheckScope,
        policy::POLICY_SCOPE_MACHINE);

    web_state_ =
        std::make_unique<web::FakeWebState>(web::WebStateID::NewUnique());
    web_state_->SetBrowserState(profile_.get());

    DataProtectionTabHelper::CreateForWebState(web_state_.get());
    tab_helper()->AddObserver(&observer_);

    fake_rt_lookup_service_ = static_cast<FakeRealTimeUrlLookupService*>(
        static_cast<safe_browsing::RealTimeUrlLookupServiceBase*>(
            RealTimeUrlLookupServiceFactory::GetForProfile(profile_.get())));
  }

  ~DataProtectionTabHelperTest() override {
    tab_helper()->RemoveObserver(&observer_);
    policy::BrowserDMTokenStorage::SetForTesting(nullptr);
  }

  std::unique_ptr<KeyedService> CreateFakeRealTimeUrlLookupService(
      ProfileIOS* profile) {
    return std::make_unique<FakeRealTimeUrlLookupService>();
  }

  DataProtectionTabHelper* tab_helper() {
    return DataProtectionTabHelper::FromWebState(web_state_.get());
  }

  std::unique_ptr<web::FakeNavigationContext> CreateNavigationContext(
      const GURL& url) {
    auto context = std::make_unique<web::FakeNavigationContext>();
    context->SetUrl(url);
    context->SetHasCommitted(true);
    return context;
  }

  void VerifyInitialProtection(const GURL& url, bool expected_enabled) {
    auto web_state =
        std::make_unique<web::FakeWebState>(web::WebStateID::NewUnique());
    web_state->SetBrowserState(profile_.get());
    web_state->SetVisibleURL(url);

    DataProtectionTabHelper::CreateForWebState(web_state.get());
    DataProtectionTabHelper* helper =
        DataProtectionTabHelper::FromWebState(web_state.get());

    EXPECT_EQ(helper->IsScreenshotProtectionEnabled(), expected_enabled);
    EXPECT_EQ(fake_rt_lookup_service_->start_lookup_count(), 0u);
  }

  void SetScreenshotBlockRule(const std::string& url_pattern) {
    data_controls::SetDataControls(profile_->GetPrefs(),
                                   {base::StringPrintf(R"({
                                "sources": { "urls": ["%s"] },
                                "restrictions": [
                                  { "class": "SCREENSHOT", "level": "BLOCK" }
                                ]
                              })",
                                                       url_pattern.c_str())});
  }

  web::WebTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<policy::FakeBrowserDMTokenStorage> fake_dm_token_storage_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::FakeWebState> web_state_;
  TestDataProtectionTabHelperObserver observer_;
  raw_ptr<FakeRealTimeUrlLookupService> fake_rt_lookup_service_;
};

// Tests that screenshot protection is enabled when Data Controls block
// screenshots. Real-time lookup should be skipped because protection is already
// latched.
TEST_F(DataProtectionTabHelperTest, DataControlsBlock) {
  GURL protected_url(kProtectedUrl);
  SetScreenshotBlockRule(kProtectedUrl);

  auto context = CreateNavigationContext(protected_url);

  tab_helper()->DidStartNavigation(web_state_.get(), context.get());

  // Still false until committed.
  EXPECT_FALSE(tab_helper()->IsScreenshotProtectionEnabled());

  tab_helper()->DidFinishNavigation(web_state_.get(), context.get());

  EXPECT_TRUE(observer_.Wait());
  EXPECT_TRUE(tab_helper()->IsScreenshotProtectionEnabled());
  EXPECT_EQ(fake_rt_lookup_service_->start_lookup_count(), 0u);
}

// Tests that the initial URL is checked when the helper is created.
TEST_F(DataProtectionTabHelperTest, InitialURLChecked) {
  SetScreenshotBlockRule(kProtectedUrl);

  VerifyInitialProtection(GURL(kProtectedUrl), /*expected_enabled=*/true);
}

// Tests that the tab helper is a no-op (no protection, no lookups) when no
// enterprise policies (Data Controls or real-time lookups) are enabled.
TEST_F(DataProtectionTabHelperTest, NoOpWhenNoPolicy) {
  // Disable real-time lookup.
  profile_->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckMode,
      enterprise_connectors::REAL_TIME_CHECK_DISABLED);

  VerifyInitialProtection(GURL(kProtectedUrl), /*expected_enabled=*/false);
}

// Tests that screenshot protection is enabled by real-time lookup.
TEST_F(DataProtectionTabHelperTest, RealTimeLookupBlock) {
  GURL protected_url(kProtectedUrl);

  auto context = CreateNavigationContext(protected_url);

  tab_helper()->DidStartNavigation(web_state_.get(), context.get());

  // Still false until committed.
  EXPECT_FALSE(tab_helper()->IsScreenshotProtectionEnabled());

  tab_helper()->DidFinishNavigation(web_state_.get(), context.get());

  // Lookup pending state after commit.
  EXPECT_TRUE(observer_.Wait());
  EXPECT_TRUE(tab_helper()->IsScreenshotProtectionEnabled());

  // Finish lookup with BLOCK.
  auto response = std::make_unique<safe_browsing::RTLookupResponse>();
  auto* threat_info = response->add_threat_info();
  threat_info->mutable_matched_url_navigation_rule()->set_block_screenshot(
      true);
  fake_rt_lookup_service_->RunResponseCallback(std::move(response));

  EXPECT_TRUE(tab_helper()->IsScreenshotProtectionEnabled());
  EXPECT_EQ(fake_rt_lookup_service_->start_lookup_count(), 1u);
}

// Tests that screenshot protection stays enabled when real-time lookup fails.
TEST_F(DataProtectionTabHelperTest, RealTimeLookupFailed) {
  GURL protected_url(kProtectedUrl);

  auto context = CreateNavigationContext(protected_url);

  tab_helper()->DidStartNavigation(web_state_.get(), context.get());

  // Still false until committed.
  EXPECT_FALSE(tab_helper()->IsScreenshotProtectionEnabled());

  tab_helper()->DidFinishNavigation(web_state_.get(), context.get());

  // Lookup pending state after commit (fail-closed).
  EXPECT_TRUE(observer_.Wait());
  EXPECT_TRUE(tab_helper()->IsScreenshotProtectionEnabled());

  // Simulate failed lookup.
  fake_rt_lookup_service_->set_is_rt_lookup_successful(false);
  fake_rt_lookup_service_->RunResponseCallback(nullptr);

  // Stays enabled because it's fail-closed.
  EXPECT_TRUE(tab_helper()->IsScreenshotProtectionEnabled());
  EXPECT_EQ(fake_rt_lookup_service_->start_lookup_count(), 1u);
}

// Tests that real-time lookup is skipped when no DM token is present.
TEST_F(DataProtectionTabHelperTest, NoDmTokenSkipsRealTimeLookup) {
  GURL unprotected_url(kUnprotectedUrl);

  // Disable real-time lookup by clearing the fake DM token.
  fake_dm_token_storage_->SetDMToken("");

  auto context = CreateNavigationContext(unprotected_url);

  tab_helper()->DidStartNavigation(web_state_.get(), context.get());
  tab_helper()->DidFinishNavigation(web_state_.get(), context.get());

  EXPECT_FALSE(tab_helper()->IsScreenshotProtectionEnabled());
  EXPECT_EQ(fake_rt_lookup_service_->start_lookup_count(), 0u);
}

// Tests that real-time lookup is skipped when the policy is disabled.
TEST_F(DataProtectionTabHelperTest, RealTimeLookupDisabled) {
  GURL unprotected_url(kUnprotectedUrl);

  // Disable real-time lookup by policy.
  profile_->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckMode,
      enterprise_connectors::REAL_TIME_CHECK_DISABLED);

  auto context = CreateNavigationContext(unprotected_url);

  tab_helper()->DidStartNavigation(web_state_.get(), context.get());
  tab_helper()->DidFinishNavigation(web_state_.get(), context.get());

  EXPECT_FALSE(tab_helper()->IsScreenshotProtectionEnabled());
  EXPECT_EQ(fake_rt_lookup_service_->start_lookup_count(), 0u);
}

// Tests that real-time lookup is skipped when the policy is not set.
TEST_F(DataProtectionTabHelperTest, RealTimeLookupNotSet) {
  GURL unprotected_url(kUnprotectedUrl);

  // Clear the real-time lookup policy.
  profile_->GetPrefs()->ClearPref(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckMode);

  auto context = CreateNavigationContext(unprotected_url);

  tab_helper()->DidStartNavigation(web_state_.get(), context.get());
  tab_helper()->DidFinishNavigation(web_state_.get(), context.get());

  EXPECT_FALSE(tab_helper()->IsScreenshotProtectionEnabled());
  EXPECT_EQ(fake_rt_lookup_service_->start_lookup_count(), 0u);
}

// Tests that real-time lookup is skipped in Incognito.
TEST_F(DataProtectionTabHelperTest, IncognitoSkipsRealTimeLookup) {
  TestProfileIOS* incognito_profile =
      profile_->CreateOffTheRecordProfileWithTestingFactories({});

  auto incognito_web_state =
      std::make_unique<web::FakeWebState>(web::WebStateID::NewUnique());
  incognito_web_state->SetBrowserState(incognito_profile);

  DataProtectionTabHelper::CreateForWebState(incognito_web_state.get());
  DataProtectionTabHelper* incognito_tab_helper =
      DataProtectionTabHelper::FromWebState(incognito_web_state.get());

  GURL protected_url(kProtectedUrl);
  data_controls::SetDataControls(
      incognito_profile->GetPrefs(),
      {base::StringPrintf(R"({
                              "sources": { "urls": ["%s"] },
                              "restrictions": [
                                { "class": "SCREENSHOT", "level": "BLOCK" }
                              ]
                            })",
                          protected_url.spec().c_str())});

  auto context = CreateNavigationContext(protected_url);

  incognito_tab_helper->DidStartNavigation(incognito_web_state.get(),
                                           context.get());
  incognito_tab_helper->DidFinishNavigation(incognito_web_state.get(),
                                            context.get());

  EXPECT_TRUE(incognito_tab_helper->IsScreenshotProtectionEnabled());
  EXPECT_EQ(RealTimeUrlLookupServiceFactory::GetForProfile(incognito_profile),
            nullptr);
}

// Tests that redirects are checked.
TEST_F(DataProtectionTabHelperTest, RedirectsChecked) {
  GURL initial_url("https://initial.com");
  GURL redirect_url(kProtectedUrl);

  SetScreenshotBlockRule(kProtectedUrl);

  web::FakeNavigationContext context;
  context.SetUrl(initial_url);

  tab_helper()->DidStartNavigation(web_state_.get(), &context);
  // Still false until committed.
  EXPECT_FALSE(tab_helper()->IsScreenshotProtectionEnabled());
  EXPECT_EQ(fake_rt_lookup_service_->start_lookup_count(), 1u);

  context.SetUrl(redirect_url);
  tab_helper()->DidRedirectNavigation(web_state_.get(), &context);

  context.SetHasCommitted(true);
  tab_helper()->DidFinishNavigation(web_state_.get(), &context);

  EXPECT_TRUE(tab_helper()->IsScreenshotProtectionEnabled());
  // No new lookup should have been triggered for the redirect because it was
  // blocked by Data Controls.
  EXPECT_EQ(fake_rt_lookup_service_->start_lookup_count(), 1u);
}

// Tests that screenshot protection remains enabled as long as there is at least
// one pending lookup for the current navigation (e.g. during redirects).
TEST_F(DataProtectionTabHelperTest, EnableProtectionDuringLookups) {
  GURL url1("https://url1.com");
  GURL url2("https://url2.com");
  GURL url3("https://url3.com");

  web::FakeNavigationContext context;
  context.SetUrl(url1);

  tab_helper()->DidStartNavigation(web_state_.get(), &context);
  EXPECT_EQ(fake_rt_lookup_service_->start_lookup_count(), 1u);

  // Redirect to url2.
  context.SetUrl(url2);
  tab_helper()->DidRedirectNavigation(web_state_.get(), &context);
  EXPECT_EQ(fake_rt_lookup_service_->start_lookup_count(), 2u);

  // Redirect to url3.
  context.SetUrl(url3);
  tab_helper()->DidRedirectNavigation(web_state_.get(), &context);
  EXPECT_EQ(fake_rt_lookup_service_->start_lookup_count(), 3u);

  context.SetHasCommitted(true);
  tab_helper()->DidFinishNavigation(web_state_.get(), &context);

  // Should be in fail-closed (pending) state.
  EXPECT_TRUE(observer_.Wait());
  EXPECT_TRUE(tab_helper()->IsScreenshotProtectionEnabled());

  auto allow_response = []() {
    auto response = std::make_unique<safe_browsing::RTLookupResponse>();
    response->add_threat_info()
        ->mutable_matched_url_navigation_rule()
        ->set_block_screenshot(false);
    return response;
  };

  // Receive ALLOW for url1.
  fake_rt_lookup_service_->RunResponseCallback(allow_response());
  // State should NOT change because we are still waiting for url2 and url3.
  EXPECT_TRUE(tab_helper()->IsScreenshotProtectionEnabled());

  // Receive ALLOW for url2.
  fake_rt_lookup_service_->RunResponseCallback(allow_response());
  // State should NOT change because we are still waiting for url3.
  EXPECT_TRUE(tab_helper()->IsScreenshotProtectionEnabled());

  // Receive ALLOW for url3.
  fake_rt_lookup_service_->RunResponseCallback(allow_response());

  // Now it should be disabled.
  EXPECT_FALSE(observer_.Wait());
  EXPECT_FALSE(tab_helper()->IsScreenshotProtectionEnabled());
}

// Tests that protection is latched if any URL in redirect chain is protected.
TEST_F(DataProtectionTabHelperTest, RedirectLatched) {
  GURL protected_url(kProtectedUrl);
  GURL final_url(kUnprotectedUrl);

  SetScreenshotBlockRule(kProtectedUrl);

  web::FakeNavigationContext context;
  context.SetUrl(protected_url);

  tab_helper()->DidStartNavigation(web_state_.get(), &context);
  EXPECT_EQ(fake_rt_lookup_service_->start_lookup_count(), 0u);

  context.SetUrl(final_url);
  tab_helper()->DidRedirectNavigation(web_state_.get(), &context);

  context.SetHasCommitted(true);
  tab_helper()->DidFinishNavigation(web_state_.get(), &context);

  EXPECT_TRUE(observer_.Wait());
  EXPECT_TRUE(tab_helper()->IsScreenshotProtectionEnabled());
  EXPECT_EQ(fake_rt_lookup_service_->start_lookup_count(), 0u);
}
