// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/banner_promo/model/default_browser_banner_promo_app_agent.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "components/google/core/common/google_util.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_test_utils.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/platform_test.h"

namespace {

// Creates the Feature Engagement Mock Tracker.
std::unique_ptr<KeyedService> BuildFeatureEngagementMockTracker(
    ProfileIOS* profile) {
  return std::make_unique<feature_engagement::test::MockTracker>();
}

}  // namespace

@interface FakeDefaultBrowserBannerAppAgentObserver
    : NSObject <DefaultBrowserBannerAppAgentObserver>

@property(nonatomic, assign) BOOL promoDisplayed;

@end

@implementation FakeDefaultBrowserBannerAppAgentObserver

- (void)displayPromoFromAppAgent:(DefaultBrowserBannerPromoAppAgent*)appAgent {
  self.promoDisplayed = YES;
}

- (void)hidePromoFromAppAgent:(DefaultBrowserBannerPromoAppAgent*)appAgent {
  self.promoDisplayed = NO;
}

@end

// Tests DefaultBrowserBannerPromoAppAgent behavior.
class DefaultBrowserBannerPromoAppAgentTest : public PlatformTest {
 protected:
  DefaultBrowserBannerPromoAppAgentTest() {
    app_state_ = [[AppState alloc] initWithStartupInformation:nil];

    agent_ = [[DefaultBrowserBannerPromoAppAgent alloc] init];
    agent_.UICurrentlySupportsPromo = YES;

    observer_ = [[FakeDefaultBrowserBannerAppAgentObserver alloc] init];
    [agent_ addObserver:observer_];

    [app_state_ addAgent:agent_];

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        feature_engagement::TrackerFactory::GetInstance(),
        base::BindRepeating(&BuildFeatureEngagementMockTracker));

    profile_ = std::move(builder).Build();

    profile_state_ = [[ProfileState alloc] initWithAppState:app_state_];
    profile_state_.profile = profile_.get();
    SetProfileStateInitStage(profile_state_, ProfileInitStage::kFinal);
    [app_state_ profileStateCreated:profile_state_];

    mock_tracker_ = static_cast<feature_engagement::test::MockTracker*>(
        feature_engagement::TrackerFactory::GetForProfile(profile_.get()));
  }

  ~DefaultBrowserBannerPromoAppAgentTest() override {
    profile_state_.profile = nullptr;
  }

  // Returns the WebStateList for the given SceneState.
  WebStateList* GetWebStateList(SceneState* scene_state) {
    return scene_state.browserProviderInterface.mainBrowserProvider.browser
        ->GetWebStateList();
  }

  // Returns the SceneState's active web state.
  web::FakeWebState* GetActiveWebState(SceneState* scene_state) {
    return static_cast<web::FakeWebState*>(
        GetWebStateList(scene_state)->GetActiveWebState());
  }

  // Creates, adds, activates, and returns a new scene state with one web state.
  FakeSceneState* SetUpAndAddSceneState() {
    FakeSceneState* scene_state =
        [[FakeSceneState alloc] initWithAppState:app_state_
                                         profile:profile_.get()];
    scene_state.activationLevel = SceneActivationLevelForegroundActive;

    [scene_state appendWebStateWithURL:url_];
    GetWebStateList(scene_state)->ActivateWebStateAt(0);

    [agent_ appState:app_state_ sceneConnected:scene_state];
    scene_state.profileState = profile_state_;

    return scene_state;
  }

  web::WebTaskEnvironment task_env_;
  base::test::ScopedFeatureList scoped_feature_list_{
      kDefaultBrowserBannerPromo};

  const GURL url_ = GURL("http://www.example.com");

  ProfileState* profile_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  AppState* app_state_;
  raw_ptr<feature_engagement::test::MockTracker> mock_tracker_;
  FakeDefaultBrowserBannerAppAgentObserver* observer_;
  // Used to verify histogram logging.
  base::HistogramTester histogram_tester_;

  DefaultBrowserBannerPromoAppAgent* agent_;
};

// Tests that the promo appears correctly when the scene state's state changes.
TEST_F(DefaultBrowserBannerPromoAppAgentTest, TestPromoAppears) {
  FakeSceneState* scene_state = SetUpAndAddSceneState();

  // Set up Feature Engagement Tracker mock.
  EXPECT_CALL(*mock_tracker_,
              ShouldTriggerHelpUI(testing::Ref(
                  feature_engagement::kIPHiOSDefaultBrowserBannerPromoFeature)))
      .WillRepeatedly(testing::Return(true));

  scene_state.UIEnabled = YES;

  EXPECT_TRUE(observer_.promoDisplayed);
  histogram_tester_.ExpectBucketCount("IOS.DefaultBrowserBannerPromo.Shown", 1,
                                      1);

  [scene_state shutdown];
  scene_state = nil;
}

// Tests that the promo appears for the required number of navigations and then
// disappears.
TEST_F(DefaultBrowserBannerPromoAppAgentTest,
       TestPromoDisappearsAfterNavigations) {
  FakeSceneState* scene_state = SetUpAndAddSceneState();

  // Set up Feature Engagement Tracker mock.
  EXPECT_CALL(*mock_tracker_,
              ShouldTriggerHelpUI(testing::Ref(
                  feature_engagement::kIPHiOSDefaultBrowserBannerPromoFeature)))
      .WillRepeatedly(testing::Return(true));

  scene_state.UIEnabled = YES;

  EXPECT_TRUE(observer_.promoDisplayed);
  histogram_tester_.ExpectBucketCount("IOS.DefaultBrowserBannerPromo.Shown", 1,
                                      1);

  // Get and set up active web state and navigation context.
  web::FakeWebState* web_state = GetActiveWebState(scene_state);

  web::FakeNavigationContext context;
  context.SetUrl(url_);
  context.SetIsSameDocument(false);

  // Navigate enough times to use up all promo views.
  for (int navigation_count = 1;
       navigation_count < kDefaultBrowserBannerPromoImpressionLimit.Get();
       navigation_count++) {
    web_state->OnNavigationFinished(&context);

    EXPECT_TRUE(observer_.promoDisplayed);
    // Bucket is 1-indexed.
    histogram_tester_.ExpectBucketCount("IOS.DefaultBrowserBannerPromo.Shown",
                                        navigation_count + 1, 1);
  }

  // Next navigation should cause the promo to hide.
  EXPECT_CALL(
      *mock_tracker_,
      Dismissed(testing::Ref(
          feature_engagement::kIPHiOSDefaultBrowserBannerPromoFeature)));

  web_state->OnNavigationFinished(&context);

  EXPECT_FALSE(observer_.promoDisplayed);
  histogram_tester_.ExpectBucketCount(
      "IOS.DefaultBrowserBannerPromo.PromoSessionEnded",
      IOSDefaultBrowserBannerPromoPromoSessionEndedReason::kImpressionsMet, 1);

  [scene_state shutdown];
  scene_state = nil;
}

// Tests that the promo will disappear when the close button is tapped.
TEST_F(DefaultBrowserBannerPromoAppAgentTest,
       TestPromoDisappearsAfterUserCloses) {
  FakeSceneState* scene_state = SetUpAndAddSceneState();

  // Set up Feature Engagement Tracker mock.
  EXPECT_CALL(*mock_tracker_,
              ShouldTriggerHelpUI(testing::Ref(
                  feature_engagement::kIPHiOSDefaultBrowserBannerPromoFeature)))
      .WillRepeatedly(testing::Return(true));

  scene_state.UIEnabled = YES;

  EXPECT_TRUE(observer_.promoDisplayed);
  histogram_tester_.ExpectBucketCount("IOS.DefaultBrowserBannerPromo.Shown", 1,
                                      1);

  // Tapping the close button should dismiss the promo.
  EXPECT_CALL(
      *mock_tracker_,
      Dismissed(testing::Ref(
          feature_engagement::kIPHiOSDefaultBrowserBannerPromoFeature)));

  [agent_ promoCloseButtonTapped];

  EXPECT_FALSE(observer_.promoDisplayed);
  histogram_tester_.ExpectBucketCount(
      "IOS.DefaultBrowserBannerPromo.PromoSessionEnded",
      IOSDefaultBrowserBannerPromoPromoSessionEndedReason::kUserClosed, 1);
  histogram_tester_.ExpectBucketCount(
      "IOS.DefaultBrowserBannerPromo.ManuallyDismissed", 1, 1);

  [scene_state shutdown];
  scene_state = nil;
}

// Tests that the promo should disappear when the user regularly interacts with
// the promo.
TEST_F(DefaultBrowserBannerPromoAppAgentTest,
       TestPromoDisappearsAfterUserInteracts) {
  FakeSceneState* scene_state = SetUpAndAddSceneState();

  // Set up Feature Engagement Tracker mock.
  EXPECT_CALL(*mock_tracker_,
              ShouldTriggerHelpUI(testing::Ref(
                  feature_engagement::kIPHiOSDefaultBrowserBannerPromoFeature)))
      .WillRepeatedly(testing::Return(true));

  scene_state.UIEnabled = YES;

  EXPECT_TRUE(observer_.promoDisplayed);
  histogram_tester_.ExpectBucketCount("IOS.DefaultBrowserBannerPromo.Shown", 1,
                                      1);

  // Tapping the promo should cause it to disappear.
  EXPECT_CALL(
      *mock_tracker_,
      Dismissed(testing::Ref(
          feature_engagement::kIPHiOSDefaultBrowserBannerPromoFeature)));

  [agent_ promoTapped];

  EXPECT_FALSE(observer_.promoDisplayed);
  histogram_tester_.ExpectBucketCount(
      "IOS.DefaultBrowserBannerPromo.PromoSessionEnded",
      IOSDefaultBrowserBannerPromoPromoSessionEndedReason::kUserTappedPromo, 1);
  histogram_tester_.ExpectBucketCount("IOS.DefaultBrowserBannerPromo.Tapped", 1,
                                      1);

  [scene_state shutdown];
  scene_state = nil;
}

// Tests that the promo should disappear after the user navigates to the Google
// search results page.
TEST_F(DefaultBrowserBannerPromoAppAgentTest,
       TestPromoDisappearsAfterNavigationToSearchResultsPage) {
  FakeSceneState* scene_state = SetUpAndAddSceneState();

  // Set up Feature Engagement Tracker mock.
  EXPECT_CALL(*mock_tracker_,
              ShouldTriggerHelpUI(testing::Ref(
                  feature_engagement::kIPHiOSDefaultBrowserBannerPromoFeature)))
      .WillRepeatedly(testing::Return(true));

  scene_state.UIEnabled = YES;

  EXPECT_TRUE(observer_.promoDisplayed);
  histogram_tester_.ExpectBucketCount("IOS.DefaultBrowserBannerPromo.Shown", 1,
                                      1);

  web::FakeWebState* web_state = GetActiveWebState(scene_state);

  // Create a fake Google search results page URL.
  const GURL search_results_page_url =
      google_util::GetGoogleSearchURL(GURL("https://www.google.com"));

  web::FakeNavigationContext context;
  context.SetUrl(search_results_page_url);
  context.SetIsSameDocument(false);

  // Navigation to the search results page should cause the promo to hide.
  EXPECT_CALL(
      *mock_tracker_,
      Dismissed(testing::Ref(
          feature_engagement::kIPHiOSDefaultBrowserBannerPromoFeature)));

  web_state->OnNavigationFinished(&context);

  EXPECT_FALSE(observer_.promoDisplayed);
  histogram_tester_.ExpectBucketCount(
      "IOS.DefaultBrowserBannerPromo.PromoSessionEnded",
      IOSDefaultBrowserBannerPromoPromoSessionEndedReason::kNavigationToSRP, 1);

  [scene_state shutdown];
  scene_state = nil;
}

// Tests that the promo should disappear after the user navigates to the new tab
// page.
TEST_F(DefaultBrowserBannerPromoAppAgentTest,
       TestPromoDisappearsAfterNavigationToNewTabPage) {
  FakeSceneState* scene_state = SetUpAndAddSceneState();

  // Set up Feature Engagement Tracker mock.
  EXPECT_CALL(*mock_tracker_,
              ShouldTriggerHelpUI(testing::Ref(
                  feature_engagement::kIPHiOSDefaultBrowserBannerPromoFeature)))
      .WillRepeatedly(testing::Return(true));

  scene_state.UIEnabled = YES;

  EXPECT_TRUE(observer_.promoDisplayed);
  histogram_tester_.ExpectBucketCount("IOS.DefaultBrowserBannerPromo.Shown", 1,
                                      1);

  // Set up a navigation to the new tab page.
  web::FakeWebState* web_state = GetActiveWebState(scene_state);

  web::FakeNavigationContext context;
  context.SetUrl(GURL(kChromeUIAboutNewTabURL));
  context.SetIsSameDocument(false);

  // Next navigation should cause the promo to hide.
  EXPECT_CALL(
      *mock_tracker_,
      Dismissed(testing::Ref(
          feature_engagement::kIPHiOSDefaultBrowserBannerPromoFeature)));

  web_state->OnNavigationFinished(&context);

  EXPECT_FALSE(observer_.promoDisplayed);
  histogram_tester_.ExpectBucketCount(
      "IOS.DefaultBrowserBannerPromo.PromoSessionEnded",
      IOSDefaultBrowserBannerPromoPromoSessionEndedReason::kNavigationToNTP, 1);

  [scene_state shutdown];
  scene_state = nil;
}

// Tests that the AppAgent will switch active web states and observe the
// navigation in the active web state.
TEST_F(DefaultBrowserBannerPromoAppAgentTest,
       TestAppAgentCanSwitchActiveWebState) {
  FakeSceneState* scene_state = SetUpAndAddSceneState();

  // Set up Feature Engagement Tracker mock.
  EXPECT_CALL(*mock_tracker_,
              ShouldTriggerHelpUI(testing::Ref(
                  feature_engagement::kIPHiOSDefaultBrowserBannerPromoFeature)))
      .WillRepeatedly(testing::Return(true));

  scene_state.UIEnabled = YES;

  EXPECT_TRUE(observer_.promoDisplayed);
  histogram_tester_.ExpectBucketCount("IOS.DefaultBrowserBannerPromo.Shown", 1,
                                      1);

  // Navigate once in the first active web state
  int navigation_count = 1;

  web::FakeWebState* web_state = GetActiveWebState(scene_state);

  web::FakeNavigationContext context;
  context.SetUrl(url_);
  context.SetIsSameDocument(false);

  web_state->OnNavigationFinished(&context);

  EXPECT_TRUE(observer_.promoDisplayed);
  // Bucket is 1-indexed.
  histogram_tester_.ExpectBucketCount("IOS.DefaultBrowserBannerPromo.Shown",
                                      navigation_count + 1, 1);
  navigation_count++;

  // Switch to a second active webstate (which counts as a promo show).
  GURL url2("https://www.newExample.com");
  context.SetUrl(url2);
  [scene_state appendWebStateWithURL:url2];
  GetWebStateList(scene_state)->ActivateWebStateAt(1);
  web::FakeWebState* web_state_2 = GetActiveWebState(scene_state);

  EXPECT_NE(web_state_2, web_state);

  EXPECT_TRUE(observer_.promoDisplayed);
  // Bucket is 1-indexed.
  histogram_tester_.ExpectBucketCount("IOS.DefaultBrowserBannerPromo.Shown",
                                      navigation_count + 1, 1);
  navigation_count++;

  // Navigate in the second web state enough times to use up all promo views.
  for (; navigation_count < kDefaultBrowserBannerPromoImpressionLimit.Get();
       navigation_count++) {
    web_state_2->OnNavigationFinished(&context);

    EXPECT_TRUE(observer_.promoDisplayed);
    // Bucket is 1-indexed.
    histogram_tester_.ExpectBucketCount("IOS.DefaultBrowserBannerPromo.Shown",
                                        navigation_count + 1, 1);
  }

  // Next navigation should cause the promo to hide.
  EXPECT_CALL(
      *mock_tracker_,
      Dismissed(testing::Ref(
          feature_engagement::kIPHiOSDefaultBrowserBannerPromoFeature)));

  web_state_2->OnNavigationFinished(&context);

  EXPECT_FALSE(observer_.promoDisplayed);
  histogram_tester_.ExpectBucketCount(
      "IOS.DefaultBrowserBannerPromo.PromoSessionEnded",
      IOSDefaultBrowserBannerPromoPromoSessionEndedReason::kImpressionsMet, 1);

  [scene_state shutdown];
  scene_state = nil;
}

// Tests that the app agent can observe navigations in multiple scenes at once.
TEST_F(DefaultBrowserBannerPromoAppAgentTest,
       TestAppAgentCanObserveMultipleScenes) {
  FakeSceneState* scene_state = SetUpAndAddSceneState();

  // Set up Feature Engagement Tracker mock.
  EXPECT_CALL(*mock_tracker_,
              ShouldTriggerHelpUI(testing::Ref(
                  feature_engagement::kIPHiOSDefaultBrowserBannerPromoFeature)))
      .WillRepeatedly(testing::Return(true));

  scene_state.UIEnabled = YES;

  EXPECT_TRUE(observer_.promoDisplayed);
  histogram_tester_.ExpectBucketCount("IOS.DefaultBrowserBannerPromo.Shown", 1,
                                      1);

  // Navigate once in the first active scene
  int navigation_count = 1;

  web::FakeWebState* web_state = GetActiveWebState(scene_state);

  web::FakeNavigationContext context;
  context.SetUrl(url_);
  context.SetIsSameDocument(false);

  web_state->OnNavigationFinished(&context);

  EXPECT_TRUE(observer_.promoDisplayed);
  // Bucket is 1-indexed.
  histogram_tester_.ExpectBucketCount("IOS.DefaultBrowserBannerPromo.Shown",
                                      navigation_count + 1, 1);
  navigation_count++;

  // Add a second scene, which counts as a promo show.
  FakeSceneState* scene_state_2 =
      [[FakeSceneState alloc] initWithAppState:app_state_
                                       profile:profile_.get()];
  scene_state_2.UIEnabled = YES;
  scene_state_2.activationLevel = SceneActivationLevelBackground;

  [scene_state_2 appendWebStateWithURL:url_];
  GetWebStateList(scene_state_2)->ActivateWebStateAt(0);

  [agent_ appState:app_state_ sceneConnected:scene_state_2];
  scene_state_2.profileState = profile_state_;

  scene_state_2.activationLevel = SceneActivationLevelForegroundActive;

  EXPECT_TRUE(observer_.promoDisplayed);
  // Bucket is 1-indexed.
  histogram_tester_.ExpectBucketCount("IOS.DefaultBrowserBannerPromo.Shown",
                                      navigation_count + 1, 1);
  navigation_count++;

  // Navigate once more in scene 1 and 2 separately.
  web_state->OnNavigationFinished(&context);

  EXPECT_TRUE(observer_.promoDisplayed);
  // Bucket is 1-indexed.
  histogram_tester_.ExpectBucketCount("IOS.DefaultBrowserBannerPromo.Shown",
                                      navigation_count + 1, 1);
  navigation_count++;

  web::FakeWebState* web_state_2 = GetActiveWebState(scene_state);
  web_state_2->OnNavigationFinished(&context);

  EXPECT_TRUE(observer_.promoDisplayed);
  // Bucket is 1-indexed.
  histogram_tester_.ExpectBucketCount("IOS.DefaultBrowserBannerPromo.Shown",
                                      navigation_count + 1, 1);
  navigation_count++;

  // Navigate in the second scene enough times to use up all promo views.
  for (; navigation_count < kDefaultBrowserBannerPromoImpressionLimit.Get();
       navigation_count++) {
    web_state_2->OnNavigationFinished(&context);

    EXPECT_TRUE(observer_.promoDisplayed);
    // Bucket is 1-indexed.
    histogram_tester_.ExpectBucketCount("IOS.DefaultBrowserBannerPromo.Shown",
                                        navigation_count + 1, 1);
  }

  // Next navigation should cause the promo to hide.
  EXPECT_CALL(
      *mock_tracker_,
      Dismissed(testing::Ref(
          feature_engagement::kIPHiOSDefaultBrowserBannerPromoFeature)));

  web_state_2->OnNavigationFinished(&context);

  EXPECT_FALSE(observer_.promoDisplayed);
  histogram_tester_.ExpectBucketCount(
      "IOS.DefaultBrowserBannerPromo.PromoSessionEnded",
      IOSDefaultBrowserBannerPromoPromoSessionEndedReason::kImpressionsMet, 1);

  // Check expectations now, since destroying the FakeSceneState will cause
  // more methods to be called.
  testing::Mock::VerifyAndClearExpectations(mock_tracker_);

  [scene_state_2 shutdown];
  scene_state_2 = nil;

  [scene_state shutdown];
  scene_state = nil;
}

// Tests that the promo doesn't reappear on subsequent navigations after being
// dismissed.
TEST_F(DefaultBrowserBannerPromoAppAgentTest, TestPromoDoesNotReappear) {
  FakeSceneState* scene_state = SetUpAndAddSceneState();

  // Set up Feature Engagement Tracker mock.
  EXPECT_CALL(*mock_tracker_,
              ShouldTriggerHelpUI(testing::Ref(
                  feature_engagement::kIPHiOSDefaultBrowserBannerPromoFeature)))
      .WillOnce(testing::Return(true));

  scene_state.UIEnabled = YES;

  EXPECT_TRUE(observer_.promoDisplayed);
  histogram_tester_.ExpectBucketCount("IOS.DefaultBrowserBannerPromo.Shown", 1,
                                      1);

  // Tapping the close button should dismiss the promo.
  EXPECT_CALL(
      *mock_tracker_,
      Dismissed(testing::Ref(
          feature_engagement::kIPHiOSDefaultBrowserBannerPromoFeature)));

  [agent_ promoCloseButtonTapped];

  EXPECT_FALSE(observer_.promoDisplayed);
  histogram_tester_.ExpectBucketCount(
      "IOS.DefaultBrowserBannerPromo.PromoSessionEnded",
      IOSDefaultBrowserBannerPromoPromoSessionEndedReason::kUserClosed, 1);
  histogram_tester_.ExpectBucketCount(
      "IOS.DefaultBrowserBannerPromo.ManuallyDismissed", 1, 1);

  // Navigate and ensure that promo does not show again.
  web::FakeNavigationContext context;
  context.SetUrl(url_);
  context.SetIsSameDocument(false);

  // Make sure to change feature engagement mocked response.
  EXPECT_CALL(*mock_tracker_,
              ShouldTriggerHelpUI(testing::Ref(
                  feature_engagement::kIPHiOSDefaultBrowserBannerPromoFeature)))
      .WillOnce(testing::Return(false));

  web::FakeWebState* web_state = GetActiveWebState(scene_state);
  web_state->OnNavigationFinished(&context);

  EXPECT_FALSE(observer_.promoDisplayed);

  [scene_state shutdown];
  scene_state = nil;
}
