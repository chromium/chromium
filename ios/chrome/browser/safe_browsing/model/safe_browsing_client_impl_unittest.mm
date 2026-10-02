// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/safe_browsing_client_impl.h"

#import "base/test/bind.h"
#import "base/test/scoped_feature_list.h"
#import "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#import "components/enterprise/connectors/core/common.h"
#import "components/enterprise/connectors/core/connectors_prefs.h"
#import "components/policy/core/common/policy_types.h"
#import "components/prefs/pref_service.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/security_interstitials/core/unsafe_resource.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_service_factory.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/prerender/model/prerender_tab_helper.h"
#import "ios/chrome/browser/prerender/model/prerender_tab_helper_delegate.h"
#import "ios/chrome/browser/safe_browsing/model/client_side_detection/client_side_detection_host_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/navigation/referrer.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "ui/base/page_transition_types.h"

class SafeBrowsingClientImplTest : public PlatformTest,
                                   public PrerenderTabHelperDelegate {
 public:
  SafeBrowsingClientImplTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        OptimizationGuideServiceFactory::GetInstance(),
        OptimizationGuideServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();
    web_state_ = std::make_unique<web::FakeWebState>();
    web_state_->SetBrowserState(profile_.get());
    client_ = std::make_unique<SafeBrowsingClientImpl>(
        /*pref_service=*/profile_->GetPrefs(),
        /*hash_real_time_service=*/nullptr,
        /*v5_get_hash_protocol_manager=*/nullptr,
        /*url_lookup_service_factory=*/
        base::BindRepeating(
            []() -> safe_browsing::RealTimeUrlLookupServiceBase* {
              return nullptr;
            }),
        enterprise_connectors::ConnectorsServiceFactory::GetForProfile(
            profile_.get()));
  }

  // Configures the owned WebState as a prerendered WebState.
  void EnablePrerender() {
    PrerenderTabHelper::CreateForWebState(web_state(), this);
  }

  // Returns the ProfileIOS.
  ProfileIOS* profile() { return profile_.get(); }

  // Returns the PrefService.
  PrefService* prefs() { return profile_->GetPrefs(); }

  // Returns the owned WebState.
  web::FakeWebState* web_state() { return web_state_.get(); }

  // Returns the SafeBrowsingClient.
  SafeBrowsingClientImpl* client() { return client_.get(); }

  // Returns whether CancelPrerender() was called.
  bool prerender_cancelled() const { return prerender_cancelled_; }

  // PrerenderTabHelperDelegate implementation.
  void CancelPrerender() override {
    PrerenderTabHelper::RemoveFromWebState(web_state_.get());
    prerender_cancelled_ = true;
  }

 private:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<ProfileIOS> profile_;
  std::unique_ptr<SafeBrowsingClientImpl> client_;
  std::unique_ptr<web::FakeWebState> web_state_;
  bool prerender_cancelled_ = false;
};

// Non prerendered webstates should not block unsafe resources.
TEST_F(SafeBrowsingClientImplTest,
       ShouldNotBlockUnsafeResourceIfNotPrerendered) {
  security_interstitials::UnsafeResource unsafe_resource;
  unsafe_resource.weak_web_state = web_state()->GetWeakPtr();
  EXPECT_FALSE(client()->ShouldBlockUnsafeResource(unsafe_resource));
}

// Prerendered webstates should block unsafe resources.
TEST_F(SafeBrowsingClientImplTest, ShouldBlockUnsafeResourceIfPrerendered) {
  EnablePrerender();
  security_interstitials::UnsafeResource unsafe_resource;
  unsafe_resource.weak_web_state = web_state()->GetWeakPtr();
  EXPECT_TRUE(client()->ShouldBlockUnsafeResource(unsafe_resource));
}

// Verifies prerendering is cancelled when the main frame load is cancelled.
TEST_F(SafeBrowsingClientImplTest, ShouldCancelPrerenderInMainFrame) {
  GURL url = GURL("https://www.chromium.org");
  EnablePrerender();
  EXPECT_TRUE(PrerenderTabHelper::FromWebState(web_state()));
  EXPECT_FALSE(prerender_cancelled());
  client()->OnMainFrameUrlQueryCancellationDecided(web_state(), url);
  EXPECT_FALSE(PrerenderTabHelper::FromWebState(web_state()));
  EXPECT_TRUE(prerender_cancelled());
}

// Verifies that real time url checks are forced to be synchrounous for
// Enterprise Url Filtering.
TEST_F(SafeBrowsingClientImplTest, ShouldForceSyncRealTimeUrlChecks) {
  EXPECT_FALSE(client()->ShouldForceSyncRealTimeUrlChecks());

  // Simulate the enterprise policy being enabled.
  // 1. Set the preference backing the policy to enabled.
  prefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckMode,
      enterprise_connectors::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);

  // 2. Set the preference backing the policy scope.
  prefs()->SetInteger(enterprise_connectors::kEnterpriseRealTimeUrlCheckScope,
                      policy::POLICY_SCOPE_MACHINE);

  // 3. Set up a fake browser DM token and client ID, which are required for
  //    enterprise policies to be considered active.
  policy::FakeBrowserDMTokenStorage fake_browser_dm_token_storage;
  fake_browser_dm_token_storage.SetDMToken("test_dm_token");
  fake_browser_dm_token_storage.SetClientId("test_client_id");

  // With the feature flag and policy enabled (including DM token),
  // ShouldForceSyncRealTimeUrlChecks should return true.
  EXPECT_TRUE(client()->ShouldForceSyncRealTimeUrlChecks());
}

// Test that `CreateClientSideDetectionHost` returns null when the feature is
// disabled.
TEST_F(SafeBrowsingClientImplTest,
       CreateClientSideDetectionHostReturnsNullWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      safe_browsing::kClientSideDetectionEnabledIos);
  SnapshotTabHelper::CreateForWebState(web_state());
  EXPECT_EQ(client()->CreateClientSideDetectionHost(web_state()), nullptr);
}

// Test that `CreateClientSideDetectionHost` returns null when
// `SnapshotTabHelper` is missing.
TEST_F(SafeBrowsingClientImplTest,
       CreateClientSideDetectionHostReturnsNullWhenSnapshotTabHelperMissing) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      safe_browsing::kClientSideDetectionEnabledIos);
  EXPECT_EQ(client()->CreateClientSideDetectionHost(web_state()), nullptr);
}

// Test that `CreateClientSideDetectionHost` returns null when web state is
// prerendering.
TEST_F(SafeBrowsingClientImplTest,
       CreateClientSideDetectionHostReturnsNullWhenPrerendering) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      safe_browsing::kClientSideDetectionEnabledIos);
  SnapshotTabHelper::CreateForWebState(web_state());
  EnablePrerender();
  EXPECT_EQ(client()->CreateClientSideDetectionHost(web_state()), nullptr);
}

// Test that `CreateClientSideDetectionHost` returns null when the web state
// belongs to an off-the-record profile.
TEST_F(SafeBrowsingClientImplTest,
       CreateClientSideDetectionHostReturnsNullWhenOffTheRecord) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      safe_browsing::kClientSideDetectionEnabledIos);
  ProfileIOS* otr_profile = profile()->GetOffTheRecordProfile();
  auto otr_web_state = std::make_unique<web::FakeWebState>();
  otr_web_state->SetBrowserState(otr_profile);
  SnapshotTabHelper::CreateForWebState(otr_web_state.get());
  EXPECT_EQ(client()->CreateClientSideDetectionHost(otr_web_state.get()),
            nullptr);
}

// Test that `CreateClientSideDetectionHost` returns a host when all
// preconditions are met.
TEST_F(SafeBrowsingClientImplTest,
       CreateClientSideDetectionHostReturnsHostWhenEligible) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      safe_browsing::kClientSideDetectionEnabledIos);
  SnapshotTabHelper::CreateForWebState(web_state());
  EXPECT_NE(client()->CreateClientSideDetectionHost(web_state()), nullptr);
}
