// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/safe_browsing_client_impl.h"

#import "base/memory/ptr_util.h"
#import "base/test/bind.h"
#import "base/test/scoped_feature_list.h"
#import "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#import "components/enterprise/connectors/core/common.h"
#import "components/enterprise/connectors/core/connectors_prefs.h"
#import "components/policy/core/common/policy_types.h"
#import "components/prefs/pref_service.h"
#import "components/security_interstitials/core/unsafe_resource.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_service_factory.h"
#import "ios/chrome/browser/enterprise/connectors/features.h"
#import "ios/chrome/browser/prerender/model/fake_prerender_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/navigation/referrer.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "ui/base/page_transition_types.h"

class SafeBrowsingClientImplTest : public PlatformTest {
 protected:
  SafeBrowsingClientImplTest()
      : prerender_service_(base::WrapUnique(new FakePrerenderService())),
        profile_(TestProfileIOS::Builder().Build()),
        web_state_(base::WrapUnique(new web::FakeWebState())) {
    client_ = base::WrapUnique(new SafeBrowsingClientImpl(
        /*pref_service=*/profile_->GetPrefs(),
        /*hash_real_time_service=*/nullptr, prerender_service_.get(),
        /*url_lookup_service_factory=*/
        base::BindRepeating(
            []() -> safe_browsing::RealTimeUrlLookupServiceBase* {
              return nullptr;
            }),
        enterprise_connectors::ConnectorsServiceFactory::GetForProfile(
            profile_.get())));
  }

  // Configures `prerender_service_` to prerender `web_state_`.
  void PrerenderWebState() const {
    FakePrerenderService* fake_prerender_service =
        static_cast<FakePrerenderService*>(prerender_service_.get());
    fake_prerender_service->set_prerender_web_state(web_state_.get());
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<PrerenderService> prerender_service_;
  std::unique_ptr<SafeBrowsingClientImpl> client_;
  std::unique_ptr<ProfileIOS> profile_;
  std::unique_ptr<web::FakeWebState> web_state_;
};

// Non prerendered webstates should not block unsafe resources.
TEST_F(SafeBrowsingClientImplTest,
       ShouldNotBlockUnsafeResourceIfNotPrerendered) {
  security_interstitials::UnsafeResource unsafe_resource;
  unsafe_resource.weak_web_state = web_state_->GetWeakPtr();
  EXPECT_FALSE(client_->ShouldBlockUnsafeResource(unsafe_resource));
}

// Prerendered webstates should block unsafe resources.
TEST_F(SafeBrowsingClientImplTest, ShouldBlockUnsafeResourceIfPrerendered) {
  PrerenderWebState();
  security_interstitials::UnsafeResource unsafe_resource;
  unsafe_resource.weak_web_state = web_state_->GetWeakPtr();
  EXPECT_TRUE(client_->ShouldBlockUnsafeResource(unsafe_resource));
}

// Verifies prerendering is cancelled when the main frame load is cancelled.
TEST_F(SafeBrowsingClientImplTest, ShouldCancelPrerenderInMainFrame) {
  GURL url = GURL("https://www.chromium.org");
  PrerenderWebState();
  prerender_service_->StartPrerender(url, web::Referrer(),
                                     ui::PAGE_TRANSITION_LINK, web_state_.get(),
                                     /*immediately=*/true);
  EXPECT_TRUE(prerender_service_->IsWebStatePrerendered(web_state_.get()));
  EXPECT_TRUE(prerender_service_->HasPrerenderForUrl(url));
  client_->OnMainFrameUrlQueryCancellationDecided(web_state_.get(), url);
  EXPECT_FALSE(prerender_service_->HasPrerenderForUrl(url));
}

// Verifies that real time url checks are forced to be synchrounous for
// Enterprise Url Filtering.
TEST_F(SafeBrowsingClientImplTest, ShouldForceSyncRealTimeUrlChecks) {
  EXPECT_FALSE(client_->ShouldForceSyncRealTimeUrlChecks());

  // Enable the feature flag for iOS enterprise real-time URL filtering.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      enterprise_connectors::kIOSEnterpriseRealtimeUrlFiltering);

  // Simulate the enterprise policy being enabled.
  // 1. Set the preference backing the policy to enabled.
  profile_->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckMode,
      enterprise_connectors::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);

  // 2. Set the preference backing the policy scope.
  profile_->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckScope,
      policy::POLICY_SCOPE_MACHINE);

  // 3. Set up a fake browser DM token and client ID, which are required for
  //    enterprise policies to be considered active.
  policy::FakeBrowserDMTokenStorage fake_browser_dm_token_storage;
  fake_browser_dm_token_storage.SetDMToken("test_dm_token");
  fake_browser_dm_token_storage.SetClientId("test_client_id");

  // With the feature flag and policy enabled (including DM token),
  // ShouldForceSyncRealTimeUrlChecks should return true.
  EXPECT_TRUE(client_->ShouldForceSyncRealTimeUrlChecks());
}
