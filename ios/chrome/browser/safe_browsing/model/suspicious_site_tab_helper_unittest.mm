// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/suspicious_site_tab_helper.h"

#import <Foundation/Foundation.h>

#import <memory>
#import <string>
#import <string_view>

#import "base/memory/raw_ptr.h"
#import "base/test/run_until.h"
#import "base/test/test_future.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/safe_browsing/core/browser/suspicious_site_warning_allowlist.h"
#import "components/safe_browsing/ios/browser/safe_browsing_url_allow_list.h"
#import "components/security_interstitials/core/unsafe_resource.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/safe_browsing/model/suspicious_site_tab_helper_delegate.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/components/security_interstitials/safe_browsing/fake_safe_browsing_client.h"
#import "ios/components/security_interstitials/safe_browsing/fake_safe_browsing_service.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_query_manager.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_tab_helper.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_unsafe_resource_container.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "net/base/apple/url_conversions.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "ui/base/page_transition_types.h"
#import "url/gurl.h"

namespace {

constexpr std::string_view kSuspiciousUrl =
    "https://suspicious.chromium.test/page";
constexpr std::string_view kSuspiciousHost = "suspicious.chromium.test";
constexpr std::string_view kOtherSafeUrl =
    "https://another-safe.chromium.test/";

}  // namespace

// Fake `SuspiciousSiteTabHelperDelegate` that records presentation and
// dismissal calls.
@interface FakeSuspiciousSiteTabHelperDelegate
    : NSObject <SuspiciousSiteTabHelperDelegate>
@property(nonatomic, assign) NSUInteger showCount;
@property(nonatomic, assign) NSUInteger dismissCount;
@property(nonatomic, assign) BOOL warningVisible;
@end

@implementation FakeSuspiciousSiteTabHelperDelegate

- (void)showSuspiciousSiteWarning {
  self.showCount++;
  self.warningVisible = YES;
}

- (void)dismissSuspiciousSiteWarning {
  self.dismissCount++;
  self.warningVisible = NO;
}

@end

class SuspiciousSiteTabHelperTest : public PlatformTest {
 protected:
  SuspiciousSiteTabHelperTest()
      : profile_(TestProfileIOS::Builder().Build()),
        client_(profile_->GetPrefs()),
        web_state_(std::make_unique<web::FakeWebState>()),
        delegate_([[FakeSuspiciousSiteTabHelperDelegate alloc] init]) {
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager_ = navigation_manager.get();
    web_state_->SetNavigationManager(std::move(navigation_manager));
    web_state_->SetBrowserState(profile_.get());
    web_state_->WasShown();

    SafeBrowsingQueryManager::CreateForWebState(web_state_.get(), &client_);
    SafeBrowsingTabHelper::CreateForWebState(web_state_.get(), &client_);
    SafeBrowsingUrlAllowList::CreateForWebState(web_state_.get());
    SafeBrowsingUnsafeResourceContainer::CreateForWebState(web_state_.get());
    SuspiciousSiteTabHelper::CreateForWebState(
        web_state_.get(),
        ios::HostContentSettingsMapFactory::GetForProfile(profile_.get()));
    tab_helper()->SetDelegate(delegate_);
  }

  SuspiciousSiteTabHelper* tab_helper() {
    return SuspiciousSiteTabHelper::FromWebState(web_state_.get());
  }

  scoped_refptr<HostContentSettingsMap> host_content_settings_map() {
    return ios::HostContentSettingsMapFactory::GetForProfile(profile_.get());
  }

  // Builds a suspicious site `UnsafeResource` for `url`.
  security_interstitials::UnsafeResource MakeSuspiciousResource(
      const GURL& url) {
    security_interstitials::UnsafeResource resource;
    resource.url = url;
    resource.threat_type =
        safe_browsing::SBThreatType::SB_THREAT_TYPE_WARNABLE_SUSPICIOUS_SITE;
    resource.weak_web_state = web_state_->GetWeakPtr();
    return resource;
  }

  // Starts a main-frame policy request for `url` and notifies `web_state_` that
  // the navigation started.
  void StartMainFrameNavigation(const GURL& url) {
    web::FakeNavigationContext context;
    context.SetUrl(url);
    context.SetIsSameDocument(false);
    web_state_->OnNavigationStarted(&context);

    const web::WebStatePolicyDecider::RequestInfo request_info(
        ui::PageTransition::PAGE_TRANSITION_TYPED,
        /*target_frame_is_main=*/true,
        /*target_frame_is_cross_origin=*/false,
        /*target_window_is_cross_origin=*/false,
        /*is_user_initiated=*/true, /*user_tapped_recently=*/true);
    base::test::TestFuture<web::WebStatePolicyDecider::PolicyDecision>
        decision_future;
    web_state_->ShouldAllowRequest(
        [NSURLRequest requestWithURL:net::NSURLWithGURL(url)], request_info,
        decision_future.GetCallback());
    EXPECT_TRUE(decision_future.Wait());
  }

  // Evaluates the main-frame response policy decision for `url` and commits the
  // navigation item in `navigation_manager_` and `web_state_`.
  void CommitMainFrameNavigation(const GURL& url) {
    NSURLResponse* response =
        [[NSURLResponse alloc] initWithURL:net::NSURLWithGURL(url)
                                  MIMEType:@"text/html"
                     expectedContentLength:0
                          textEncodingName:nil];
    web::WebStatePolicyDecider::ResponseInfo response_info(
        /*for_main_frame=*/true);
    base::test::TestFuture<web::WebStatePolicyDecider::PolicyDecision>
        decision_future;
    web_state_->ShouldAllowResponse(response, response_info,
                                    decision_future.GetCallback());
    EXPECT_TRUE(decision_future.Wait());

    navigation_manager_->AddItem(url, ui::PAGE_TRANSITION_TYPED);
    const int last_index = navigation_manager_->GetItemCount() - 1;
    navigation_manager_->SetLastCommittedItemIndex(last_index);
    web_state_->SetCurrentURL(url);

    web::FakeNavigationContext context;
    context.SetUrl(url);
    context.SetHasCommitted(true);
    context.SetIsSameDocument(false);
    web_state_->OnNavigationFinished(&context);
  }

  // Runs a full main-frame navigation to `url` where both sync and async Safe
  // Browsing checks complete and the navigation commits.
  void CompleteSafeNavigation(const GURL& url) {
    StartMainFrameNavigation(url);
    client_.run_sync_callbacks();
    client_.run_async_callbacks();
    CommitMainFrameNavigation(url);
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  FakeSafeBrowsingClient client_;
  std::unique_ptr<web::FakeWebState> web_state_;
  raw_ptr<web::FakeNavigationManager> navigation_manager_ = nullptr;
  FakeSuspiciousSiteTabHelperDelegate* delegate_ = nil;
};

// Tests that a suspicious site warning is presented once the navigation has
// committed and all pending main-frame Safe Browsing checks have finished.
TEST_F(SuspiciousSiteTabHelperTest,
       ShowsWarningWhenTriggeredAfterCommitAndChecksComplete) {
  const GURL url(kSuspiciousUrl);

  StartMainFrameNavigation(url);
  tab_helper()->OnSuspiciousSiteWarningTriggered(MakeSuspiciousResource(url));
  client_.run_sync_callbacks();
  client_.run_async_callbacks();
  CommitMainFrameNavigation(url);

  EXPECT_EQ(delegate_.showCount, 1u);
  EXPECT_TRUE(delegate_.warningVisible);
}

// Tests that the warning is deferred until `DidFinishNavigation` commits the
// navigation even if all Safe Browsing checks have already completed.
TEST_F(SuspiciousSiteTabHelperTest, DefersWarningUntilNavigationCommits) {
  const GURL url(kSuspiciousUrl);

  StartMainFrameNavigation(url);
  tab_helper()->OnSuspiciousSiteWarningTriggered(MakeSuspiciousResource(url));
  client_.run_sync_callbacks();
  client_.run_async_callbacks();

  // Checks are complete, but the navigation has not committed yet.
  EXPECT_EQ(delegate_.showCount, 0u);

  CommitMainFrameNavigation(url);
  EXPECT_EQ(delegate_.showCount, 1u);
  EXPECT_TRUE(delegate_.warningVisible);
}

// Tests that the warning is deferred after commit while an async Safe Browsing
// check is still pending, and presented once the async check finishes safe.
TEST_F(SuspiciousSiteTabHelperTest, DefersWarningUntilAsyncCheckCompletes) {
  const GURL url(kSuspiciousUrl);

  StartMainFrameNavigation(url);
  tab_helper()->OnSuspiciousSiteWarningTriggered(MakeSuspiciousResource(url));
  client_.run_sync_callbacks();
  CommitMainFrameNavigation(url);

  // Sync check finished and navigation committed, but async check is still in
  // flight.
  EXPECT_EQ(delegate_.showCount, 0u);

  client_.run_async_callbacks();
  EXPECT_EQ(delegate_.showCount, 1u);
  EXPECT_TRUE(delegate_.warningVisible);
}

// Tests that a pending suspicious site warning is suppressed and cleared if an
// async Safe Browsing check on the redirect chain triggers a blocking
// interstitial.
TEST_F(SuspiciousSiteTabHelperTest,
       SuppressesWarningWhenBlockingInterstitialCancelsNavigation) {
  const GURL url("https://" + FakeSafeBrowsingService::kAsyncUnsafeHost +
                 "/phishing");

  StartMainFrameNavigation(url);
  tab_helper()->OnSuspiciousSiteWarningTriggered(MakeSuspiciousResource(url));
  client_.run_sync_callbacks();
  CommitMainFrameNavigation(url);
  EXPECT_EQ(delegate_.showCount, 0u);

  // Running the async callback for `kAsyncUnsafeHost` returns `proceed=false`,
  // which triggers the blocking interstitial path and clears the redirect
  // chain in `SafeBrowsingTabHelper`.
  client_.run_async_callbacks();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !SafeBrowsingTabHelper::FromWebState(web_state_.get())
                ->IsUrlInCurrentOrPendingNavigation(url);
  }));

  EXPECT_EQ(delegate_.showCount, 0u);
  EXPECT_FALSE(delegate_.warningVisible);
}

// Tests that no warning is shown if the committed host is already on the
// 30-day suspicious site allowlist.
TEST_F(SuspiciousSiteTabHelperTest, SuppressesWarningWhenHostIsAllowlisted) {
  safe_browsing::SuspiciousSiteWarningAllowlist allowlist(
      host_content_settings_map());
  allowlist.AllowSiteForHost(std::string(kSuspiciousHost));

  const GURL url(kSuspiciousUrl);
  StartMainFrameNavigation(url);
  tab_helper()->OnSuspiciousSiteWarningTriggered(MakeSuspiciousResource(url));
  client_.run_sync_callbacks();
  client_.run_async_callbacks();
  CommitMainFrameNavigation(url);

  EXPECT_EQ(delegate_.showCount, 0u);
  EXPECT_FALSE(delegate_.warningVisible);
}

// Tests that a warning triggered in a hidden tab is deferred until `WasShown`
// is called.
TEST_F(SuspiciousSiteTabHelperTest,
       DefersWarningWhileHiddenAndPresentsWhenShown) {
  web_state_->WasHidden();

  const GURL url(kSuspiciousUrl);
  StartMainFrameNavigation(url);
  tab_helper()->OnSuspiciousSiteWarningTriggered(MakeSuspiciousResource(url));
  client_.run_sync_callbacks();
  client_.run_async_callbacks();
  CommitMainFrameNavigation(url);

  EXPECT_EQ(delegate_.showCount, 0u);

  web_state_->WasShown();
  EXPECT_EQ(delegate_.showCount, 1u);
  EXPECT_TRUE(delegate_.warningVisible);
}

// Tests that hiding a tab dismisses the visible warning and showing the tab
// again re-presents the warning.
TEST_F(SuspiciousSiteTabHelperTest,
       SuspendsAndRePresentsAcrossVisibilityChanges) {
  const GURL url(kSuspiciousUrl);

  StartMainFrameNavigation(url);
  tab_helper()->OnSuspiciousSiteWarningTriggered(MakeSuspiciousResource(url));
  client_.run_sync_callbacks();
  client_.run_async_callbacks();
  CommitMainFrameNavigation(url);

  EXPECT_EQ(delegate_.showCount, 1u);
  EXPECT_EQ(delegate_.dismissCount, 0u);

  web_state_->WasHidden();
  EXPECT_EQ(delegate_.dismissCount, 1u);
  EXPECT_FALSE(delegate_.warningVisible);

  web_state_->WasShown();
  EXPECT_EQ(delegate_.showCount, 2u);
  EXPECT_TRUE(delegate_.warningVisible);
}

// Tests that manually navigating to another page while the warning is shown
// dismisses the warning.
TEST_F(SuspiciousSiteTabHelperTest,
       ManualNavigationAwayAfterShownDismissesWarning) {
  const GURL url(kSuspiciousUrl);

  StartMainFrameNavigation(url);
  tab_helper()->OnSuspiciousSiteWarningTriggered(MakeSuspiciousResource(url));
  client_.run_sync_callbacks();
  client_.run_async_callbacks();
  CommitMainFrameNavigation(url);

  ASSERT_EQ(delegate_.showCount, 1u);
  EXPECT_EQ(delegate_.dismissCount, 0u);

  CompleteSafeNavigation(GURL(kOtherSafeUrl));

  EXPECT_EQ(delegate_.dismissCount, 1u);
  EXPECT_FALSE(delegate_.warningVisible);
}
