// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/safe_browsing/url_checker_delegate_impl.h"

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/ref_counted.h"
#import "base/run_loop.h"
#import "base/test/ios/wait_util.h"
#import "base/test/run_until.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/test_future.h"
#import "components/safe_browsing/core/browser/db/database_manager.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/safe_browsing/ios/browser/safe_browsing_url_allow_list.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/components/security_interstitials/safe_browsing/fake_safe_browsing_client.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_query_manager.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_unsafe_resource_container.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "net/http/http_request_headers.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

using base::test::ios::kWaitForFileOperationTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;
using security_interstitials::UnsafeResource;

namespace {
// Struct used to test the execution of UnsafeResource callbacks.
struct UnsafeResourceCallbackState {
  bool executed = false;
  bool proceed = false;
  bool show_interstitial = false;
};
// Function used as the callback for UnsafeResources.
void PopulateCallbackState(UnsafeResourceCallbackState* state,
                           UnsafeResource::UrlCheckResult result) {
  state->executed = true;
  state->proceed = result.proceed;
  state->show_interstitial = result.showed_interstitial;
}
}  // namespace

// Test fixture for UrlCheckerDelegateImpl.
class UrlCheckerDelegateImplTest : public PlatformTest {
 public:
  UrlCheckerDelegateImplTest()
      : browser_state_(std::make_unique<web::FakeBrowserState>()),
        client_(&pref_service_),
        delegate_(
            base::MakeRefCounted<UrlCheckerDelegateImpl>(nullptr,
                                                         client_.AsWeakPtr())),
        item_(web::NavigationItem::Create()),
        web_state_(std::make_unique<web::FakeWebState>()) {
    safe_browsing::RegisterProfilePrefs(pref_service_.registry());
    // Set up the WebState.
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager->SetLastCommittedItem(item_.get());
    web_state_->SetNavigationManager(std::move(navigation_manager));
    web_state_->SetBrowserState(browser_state_.get());
    // Construct the allow list and unsafe resource container.
    SafeBrowsingUnsafeResourceContainer::CreateForWebState(web_state_.get());
    SafeBrowsingUrlAllowList::CreateForWebState(web_state_.get());
    SafeBrowsingQueryManager::CreateForWebState(web_state_.get(), &client_);
  }
  ~UrlCheckerDelegateImplTest() override = default;

  // Creates an UnsafeResource whose callback populates `callback_state`.
  UnsafeResource CreateUnsafeResource(
      UnsafeResourceCallbackState* callback_state) {
    UnsafeResource resource;
    resource.url = GURL("http://www.chromium.test");
    resource.callback_sequence = task_environment_.GetMainThreadTaskRunner();
    resource.callback =
        base::BindRepeating(&PopulateCallbackState, callback_state);
    resource.weak_web_state = web_state_->GetWeakPtr();
    return resource;
  }

  // Waits for `state.executed` to be reset to true.  Returns whether the state
  // populated before a timeout.
  bool WaitForUnsafeResourceCallbackExecution(
      UnsafeResourceCallbackState* state) {
    task_environment_.RunUntilIdle();
    return WaitUntilConditionOrTimeout(kWaitForFileOperationTimeout, ^{
      return state->executed;
    });
  }

  // Flushes any pending tasks queued on the UI thread task runner.
  void FlushUIThreadTasks() {
    base::test::TestFuture<void> flush_future;
    web::GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                             flush_future.GetCallback());
    ASSERT_TRUE(flush_future.Wait());
  }

  // Getters for the allow list and unsafe resource container.
  SafeBrowsingUrlAllowList* allow_list() {
    return web_state_ ? SafeBrowsingUrlAllowList::FromWebState(web_state_.get())
                      : nullptr;
  }
  SafeBrowsingUnsafeResourceContainer* container() {
    return web_state_ ? SafeBrowsingUnsafeResourceContainer::FromWebState(
                            web_state_.get())
                      : nullptr;
  }

 protected:
  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::MainThreadType::IO};
  std::unique_ptr<web::FakeBrowserState> browser_state_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  FakeSafeBrowsingClient client_;
  scoped_refptr<safe_browsing::UrlCheckerDelegate> delegate_;
  std::unique_ptr<web::NavigationItem> item_;
  std::unique_ptr<web::FakeWebState> web_state_;
};

// Tests that the delegate does not allow unsafe resources to proceed and does
// not show an interstitial for UnsafeResources for destroyed WebStates.
TEST_F(UrlCheckerDelegateImplTest, DontProceedForDestroyedWebState) {
  // Construct the UnsafeResource.
  UnsafeResourceCallbackState callback_state;
  UnsafeResource resource = CreateUnsafeResource(&callback_state);

  // Destroy the WebState.
  web_state_ = nullptr;

  // Instruct the delegate to display the blocking page.
  delegate_->StartDisplayingBlockingPageHelper(resource, /*method=*/"",
                                               net::HttpRequestHeaders(),
                                               /*has_user_gesture=*/true);
  EXPECT_TRUE(WaitForUnsafeResourceCallbackExecution(&callback_state));

  // Verify the callback state.
  EXPECT_FALSE(callback_state.proceed);
  EXPECT_FALSE(callback_state.show_interstitial);
}

// Tests that the delegate does not allow unsafe resources to proceed and does
// not show an interstitial for UnsafeResources if it is blocked by the client.
TEST_F(UrlCheckerDelegateImplTest, DontProceedIfBlockedByClient) {
  // Construct the UnsafeResource.
  UnsafeResourceCallbackState callback_state;
  UnsafeResource resource = CreateUnsafeResource(&callback_state);

  // Make client block unsafe resource.
  client_.set_should_block_unsafe_resource(true);

  // Instruct the delegate to display the blocking page.
  delegate_->StartDisplayingBlockingPageHelper(resource, /*method=*/"",
                                               net::HttpRequestHeaders(),
                                               /*has_user_gesture=*/true);
  EXPECT_TRUE(WaitForUnsafeResourceCallbackExecution(&callback_state));

  // Verify the callback state.
  EXPECT_FALSE(callback_state.proceed);
  EXPECT_FALSE(callback_state.show_interstitial);
}

// Tests that the delegate allows unsafe resources to proceed without showing an
// interstitial for allows unsafe navigations.
TEST_F(UrlCheckerDelegateImplTest, ProceedForAllowedUnsafeNavigation) {
  // Construct the UnsafeResource.
  safe_browsing::SBThreatType threat_type =
      safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_PHISHING;
  UnsafeResourceCallbackState callback_state;
  UnsafeResource resource = CreateUnsafeResource(&callback_state);
  resource.threat_type = threat_type;

  // Add the resource to the allow list.
  allow_list()->AllowUnsafeNavigations(resource.url, threat_type);

  // Instruct the delegate to display the blocking page.
  delegate_->StartDisplayingBlockingPageHelper(resource, /*method=*/"",
                                               net::HttpRequestHeaders(),
                                               /*has_user_gesture=*/true);
  EXPECT_TRUE(WaitForUnsafeResourceCallbackExecution(&callback_state));

  // Verify the callback state.
  EXPECT_TRUE(callback_state.proceed);
  EXPECT_FALSE(callback_state.show_interstitial);
}

// Tests that suspicious site warnings are allowed and forwarded to the client
// asynchronously when both Enhanced Safe Browsing and `kSuspiciousSiteWarnings`
// are enabled.
TEST_F(UrlCheckerDelegateImplTest,
       ShowSuspiciousSiteWarningAllowedWhenEnhancedAndFeatureEnabled) {
  base::test::ScopedFeatureList feature_list(
      safe_browsing::kSuspiciousSiteWarnings);
  pref_service_.SetBoolean(prefs::kSafeBrowsingEnhanced, true);

  EXPECT_TRUE(
      delegate_->AreSuspiciousSiteWarningsAllowed(base::NullCallback()));

  UnsafeResource resource;
  resource.url = GURL("https://suspicious.chromium.test");
  resource.threat_type =
      safe_browsing::SBThreatType::SB_THREAT_TYPE_WARNABLE_SUSPICIOUS_SITE;
  resource.weak_web_state = web_state_->GetWeakPtr();

  delegate_->ShowSuspiciousSiteWarning(resource, base::NullCallback());
  EXPECT_FALSE(client_.on_suspicious_site_warning_triggered_called());
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return client_.on_suspicious_site_warning_triggered_called(); }));
  EXPECT_EQ(resource.url, client_.last_suspicious_site_url());
}

// Tests that suspicious site warnings are suppressed when Enhanced Safe
// Browsing is disabled.
TEST_F(UrlCheckerDelegateImplTest,
       ShowSuspiciousSiteWarningSuppressedWithoutEnhancedProtection) {
  base::test::ScopedFeatureList feature_list(
      safe_browsing::kSuspiciousSiteWarnings);
  pref_service_.SetBoolean(prefs::kSafeBrowsingEnabled, true);
  pref_service_.SetBoolean(prefs::kSafeBrowsingEnhanced, false);

  EXPECT_FALSE(
      delegate_->AreSuspiciousSiteWarningsAllowed(base::NullCallback()));

  UnsafeResource resource;
  resource.url = GURL("https://suspicious.chromium.test");
  resource.threat_type =
      safe_browsing::SBThreatType::SB_THREAT_TYPE_WARNABLE_SUSPICIOUS_SITE;
  resource.weak_web_state = web_state_->GetWeakPtr();

  delegate_->ShowSuspiciousSiteWarning(resource, base::NullCallback());
  FlushUIThreadTasks();
  EXPECT_FALSE(client_.on_suspicious_site_warning_triggered_called());
}

// Tests that suspicious site warnings are suppressed when
// `kSuspiciousSiteWarnings` is disabled.
TEST_F(UrlCheckerDelegateImplTest,
       ShowSuspiciousSiteWarningSuppressedWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(safe_browsing::kSuspiciousSiteWarnings);
  pref_service_.SetBoolean(prefs::kSafeBrowsingEnhanced, true);

  EXPECT_FALSE(
      delegate_->AreSuspiciousSiteWarningsAllowed(base::NullCallback()));

  UnsafeResource resource;
  resource.url = GURL("https://suspicious.chromium.test");
  resource.threat_type =
      safe_browsing::SBThreatType::SB_THREAT_TYPE_WARNABLE_SUSPICIOUS_SITE;
  resource.weak_web_state = web_state_->GetWeakPtr();

  delegate_->ShowSuspiciousSiteWarning(resource, base::NullCallback());
  FlushUIThreadTasks();
  EXPECT_FALSE(client_.on_suspicious_site_warning_triggered_called());
}

// Tests that suspicious site warnings are suppressed in off-the-record
// `WebState`s even when Enhanced Safe Browsing and `kSuspiciousSiteWarnings`
// are enabled.
TEST_F(UrlCheckerDelegateImplTest,
       ShowSuspiciousSiteWarningSuppressedInOffTheRecord) {
  base::test::ScopedFeatureList feature_list(
      safe_browsing::kSuspiciousSiteWarnings);
  pref_service_.SetBoolean(prefs::kSafeBrowsingEnhanced, true);
  browser_state_->SetOffTheRecord(true);

  UnsafeResource resource;
  resource.url = GURL("https://suspicious.chromium.test");
  resource.threat_type =
      safe_browsing::SBThreatType::SB_THREAT_TYPE_WARNABLE_SUSPICIOUS_SITE;
  resource.weak_web_state = web_state_->GetWeakPtr();

  delegate_->ShowSuspiciousSiteWarning(resource, base::NullCallback());
  FlushUIThreadTasks();
  EXPECT_FALSE(client_.on_suspicious_site_warning_triggered_called());
}

// Tests that suspicious site warnings are suppressed when the client blocks the
// resource (for example, in prerender `WebState`s).
TEST_F(UrlCheckerDelegateImplTest,
       ShowSuspiciousSiteWarningSuppressedWhenBlockedByClient) {
  base::test::ScopedFeatureList feature_list(
      safe_browsing::kSuspiciousSiteWarnings);
  pref_service_.SetBoolean(prefs::kSafeBrowsingEnhanced, true);
  client_.set_should_block_unsafe_resource(true);

  UnsafeResource resource;
  resource.url = GURL("https://suspicious.chromium.test");
  resource.threat_type =
      safe_browsing::SBThreatType::SB_THREAT_TYPE_WARNABLE_SUSPICIOUS_SITE;
  resource.weak_web_state = web_state_->GetWeakPtr();

  delegate_->ShowSuspiciousSiteWarning(resource, base::NullCallback());
  FlushUIThreadTasks();
  EXPECT_FALSE(client_.on_suspicious_site_warning_triggered_called());
}

// Tests that suspicious site warnings are suppressed when the `WebState` has
// been destroyed.
TEST_F(UrlCheckerDelegateImplTest,
       ShowSuspiciousSiteWarningSuppressedForDestroyedWebState) {
  base::test::ScopedFeatureList feature_list(
      safe_browsing::kSuspiciousSiteWarnings);
  pref_service_.SetBoolean(prefs::kSafeBrowsingEnhanced, true);

  UnsafeResource resource;
  resource.url = GURL("https://suspicious.chromium.test");
  resource.threat_type =
      safe_browsing::SBThreatType::SB_THREAT_TYPE_WARNABLE_SUSPICIOUS_SITE;
  resource.weak_web_state = web_state_->GetWeakPtr();
  web_state_ = nullptr;

  delegate_->ShowSuspiciousSiteWarning(resource, base::NullCallback());
  FlushUIThreadTasks();
  EXPECT_FALSE(client_.on_suspicious_site_warning_triggered_called());
}
