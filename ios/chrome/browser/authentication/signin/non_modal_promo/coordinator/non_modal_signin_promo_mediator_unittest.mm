// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/signin/non_modal_promo/coordinator/non_modal_signin_promo_mediator.h"

#import <memory>
#import <string>

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "components/infobars/core/confirm_infobar_delegate.h"
#import "ios/chrome/browser/authentication/signin/non_modal_promo/coordinator/non_modal_signin_promo_metrics_util.h"
#import "ios/chrome/browser/authentication/signin/non_modal_promo/coordinator/non_modal_signin_promo_types.h"
#import "ios/chrome/browser/browser_view/model/browser_view_visibility_notifier_browser_agent.h"
#import "ios/chrome/browser/browser_view/public/browser_view_visibility_state.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/overlays/model/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/infobar_banner/infobar_banner_overlay_responses.h"
#import "ios/chrome/browser/overlays/model/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/model/public/overlay_response.h"
#import "ios/chrome/browser/overlays/model/test/fake_overlay_presentation_context.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Fake implementation of `NonModalSignInPromoMediatorDelegate` for testing.
@interface FakeNonModalSignInPromoMediatorDelegate
    : NSObject <NonModalSignInPromoMediatorDelegate>

@property(nonatomic, assign) BOOL canDisplayPromo;
@property(nonatomic, assign) NSInteger didAcceptCount;
@property(nonatomic, assign) NSInteger shouldDismissCount;
@property(nonatomic, weak) NonModalSignInPromoMediator* lastMediator;

@end

@implementation FakeNonModalSignInPromoMediatorDelegate

- (instancetype)init {
  self = [super init];
  if (self) {
    _canDisplayPromo = YES;
  }
  return self;
}

- (BOOL)nonModalSignInPromoMediatorCanDisplayPromo:
    (NonModalSignInPromoMediator*)mediator {
  return _canDisplayPromo;
}

- (void)nonModalSignInPromoMediatorDidAccept:
    (NonModalSignInPromoMediator*)mediator {
  _didAcceptCount++;
  _lastMediator = mediator;
}

- (void)nonModalSignInPromoMediatorShouldDismiss:
    (NonModalSignInPromoMediator*)mediator {
  _shouldDismissCount++;
  _lastMediator = mediator;
}

@end

namespace {

// Histogram recording the actions on the password promo.
constexpr char kPasswordPromoHistogram[] = "IOS.SignInPromo.NonModal.Password";

// Minimal non-promo `ConfirmInfoBarDelegate` for testing interaction with other
// infobars.
class FakeOtherInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  InfoBarIdentifier GetIdentifier() const override { return TEST_INFOBAR; }
  std::u16string GetMessageText() const override { return u"Other infobar"; }
  bool EqualsDelegate(InfoBarDelegate* delegate) const override {
    return false;
  }
};

class NonModalSignInPromoMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    identity_ = [FakeSystemIdentity fakeIdentity1];
    auto* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(identity_);

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindOnce(&CreateTestSyncService));
    builder.AddTestingFactory(
        feature_engagement::TrackerFactory::GetInstance(),
        base::BindOnce(
            &std::make_unique<feature_engagement::test::MockTracker>));
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));
    authentication_service_ =
        AuthenticationServiceFactory::GetForProfile(profile_);
    mock_tracker_ = static_cast<feature_engagement::test::MockTracker*>(
        feature_engagement::TrackerFactory::GetForProfile(profile_));

    browser_ = std::make_unique<TestBrowser>(profile_.get(),
                                             [[SceneState alloc] init]);
    BrowserViewVisibilityNotifierBrowserAgent::CreateForBrowser(browser_.get());
    visibility_notifier_ =
        BrowserViewVisibilityNotifierBrowserAgent::FromBrowser(browser_.get());
    overlay_presenter_ = OverlayPresenter::FromBrowser(
        browser_.get(), OverlayModality::kInfobarBanner);
    overlay_presenter_->SetPresentationContext(&overlay_presentation_context_);
    InsertWebState();

    fake_delegate_ = [[FakeNonModalSignInPromoMediatorDelegate alloc] init];
    mediator_ = [[NonModalSignInPromoMediator alloc]
        initWithAuthenticationService:authentication_service_
                      identityManager:IdentityManagerFactory::GetForProfile(
                                          profile_)
             featureEngagementTracker:mock_tracker_
                         webStateList:browser_->GetWebStateList()
                     overlayPresenter:overlay_presenter_
        browserViewVisibilityNotifier:visibility_notifier_
                            promoType:NonModalSignInPromoType::kPassword];
    mediator_.delegate = fake_delegate_;
  }

  void TearDown() override {
    [mediator_ disconnect];
    mediator_ = nil;
    overlay_presenter_->SetPresentationContext(nullptr);
    overlay_presenter_ = nullptr;
    visibility_notifier_ = nullptr;
    mock_tracker_ = nullptr;
    authentication_service_ = nullptr;
    profile_ = nullptr;

    PlatformTest::TearDown();
  }

  // Inserts and activates a WebState that supports infobars.
  void InsertWebState() {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    InfoBarManagerImpl::CreateForWebState(web_state.get());
    browser_->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate());
  }

  // Makes the FET tracker allow the password promo.
  void AllowPromo() {
    EXPECT_CALL(
        *mock_tracker_,
        WouldTriggerHelpUI(testing::Ref(
            feature_engagement::kIPHiOSPromoNonModalSigninPasswordFeature)))
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(
        *mock_tracker_,
        ShouldTriggerHelpUI(testing::Ref(
            feature_engagement::kIPHiOSPromoNonModalSigninPasswordFeature)))
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(*mock_tracker_, Dismissed(testing::_))
        .Times(testing::AnyNumber());
  }

  // Starts the display timer and fast-forwards past the display delay.
  void StartPromo() {
    [mediator_ startPromoDisplayTimer];
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Returns the promo infobar of the active WebState, if any.
  InfoBarIOS* GetPromoInfobar() {
    InfoBarManagerImpl* manager = InfoBarManagerImpl::FromWebState(
        browser_->GetWebStateList()->GetActiveWebState());
    for (infobars::InfoBar* infobar : manager->infobars()) {
      if (infobar->delegate()->GetIdentifier() ==
          infobars::InfoBarDelegate::
              NON_MODAL_SIGNIN_PROMO_INFOBAR_DELEGATE_IOS) {
        return static_cast<InfoBarIOS*>(infobar);
      }
    }
    return nullptr;
  }

  // Adds a non-promo infobar to the active WebState and returns it.
  InfoBarIOS* AddOtherInfoBar(bool skip_banner) {
    InfoBarManagerImpl* manager = InfoBarManagerImpl::FromWebState(
        browser_->GetWebStateList()->GetActiveWebState());
    return static_cast<InfoBarIOS*>(
        manager->AddInfoBar(std::make_unique<InfoBarIOS>(
            InfobarType::kInfobarTypeConfirm,
            std::make_unique<FakeOtherInfoBarDelegate>(), skip_banner)));
  }

  // Presents the banner of `infobar` in the active WebState and returns its
  // request.
  OverlayRequest* PresentBanner(InfoBarIOS* infobar) {
    std::unique_ptr<OverlayRequest> request =
        OverlayRequest::CreateWithConfig<InfobarOverlayRequestConfig>(
            infobar, InfobarOverlayType::kBanner, infobar->high_priority());
    OverlayRequest* request_ptr = request.get();
    OverlayRequestQueue::FromWebState(
        browser_->GetWebStateList()->GetActiveWebState(),
        OverlayModality::kInfobarBanner)
        ->AddRequest(std::move(request));
    return request_ptr;
  }

  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::TimeSource::MOCK_TIME};
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_ = nullptr;
  FakeSystemIdentity* identity_ = nil;
  raw_ptr<AuthenticationService> authentication_service_ = nullptr;
  raw_ptr<feature_engagement::test::MockTracker> mock_tracker_ = nullptr;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<BrowserViewVisibilityNotifierBrowserAgent> visibility_notifier_ =
      nullptr;
  raw_ptr<OverlayPresenter> overlay_presenter_ = nullptr;
  FakeOverlayPresentationContext overlay_presentation_context_;
  FakeNonModalSignInPromoMediatorDelegate* fake_delegate_ = nil;
  NonModalSignInPromoMediator* mediator_ = nil;
};

}  // namespace

// Tests that presenting the promo banner and its removal are recorded as an
// appearance and a timeout.
TEST_F(NonModalSignInPromoMediatorTest, RecordsAppearAndTimeout) {
  base::HistogramTester histogram_tester;
  AllowPromo();
  StartPromo();
  InfoBarIOS* infobar = GetPromoInfobar();
  ASSERT_TRUE(infobar);

  PresentBanner(infobar);
  // Presenting its own banner must not dismiss the promo.
  ASSERT_TRUE(GetPromoInfobar());
  histogram_tester.ExpectBucketCount(kPasswordPromoHistogram,
                                     NonModalSignInPromoAction::kAppear, 1);

  infobar->owner()->RemoveInfoBar(infobar);
  histogram_tester.ExpectBucketCount(kPasswordPromoHistogram,
                                     NonModalSignInPromoAction::kTimeout, 1);
  EXPECT_EQ(1, fake_delegate_.shouldDismissCount);
  EXPECT_EQ(mediator_, fake_delegate_.lastMediator);
}

// Tests that the user dismissing the promo banner is recorded as a dismissal
// rather than a timeout.
TEST_F(NonModalSignInPromoMediatorTest, RecordsUserDismissal) {
  base::HistogramTester histogram_tester;
  AllowPromo();
  StartPromo();
  InfoBarIOS* infobar = GetPromoInfobar();
  ASSERT_TRUE(infobar);
  OverlayRequest* request = PresentBanner(infobar);

  request->GetCallbackManager()->DispatchResponse(
      OverlayResponse::CreateWithInfo<
          InfobarBannerUserInitiatedDismissalResponse>());
  infobar->owner()->RemoveInfoBar(infobar);

  histogram_tester.ExpectBucketCount(kPasswordPromoHistogram,
                                     NonModalSignInPromoAction::kDismiss, 1);
  histogram_tester.ExpectBucketCount(kPasswordPromoHistogram,
                                     NonModalSignInPromoAction::kTimeout, 0);
  EXPECT_EQ(1, fake_delegate_.shouldDismissCount);
}

// Tests that accepting the promo logs acceptance, notifies the delegate,
// notifies the tracker when the infobar is removed (before `-disconnect`), and
// does not log a timeout.
TEST_F(NonModalSignInPromoMediatorTest, AcceptLogsAndNotifiesDelegate) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(
      *mock_tracker_,
      WouldTriggerHelpUI(testing::Ref(
          feature_engagement::kIPHiOSPromoNonModalSigninPasswordFeature)))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(
      *mock_tracker_,
      ShouldTriggerHelpUI(testing::Ref(
          feature_engagement::kIPHiOSPromoNonModalSigninPasswordFeature)))
      .WillOnce(testing::Return(true));
  StartPromo();
  InfoBarIOS* infobar = GetPromoInfobar();
  ASSERT_TRUE(infobar);
  PresentBanner(infobar);

  EXPECT_TRUE(infobar->delegate()->AsConfirmInfoBarDelegate()->Accept());
  EXPECT_EQ(1, fake_delegate_.didAcceptCount);
  histogram_tester.ExpectBucketCount(kPasswordPromoHistogram,
                                     NonModalSignInPromoAction::kAccept, 1);

  EXPECT_CALL(
      *mock_tracker_,
      Dismissed(testing::Ref(
          feature_engagement::kIPHiOSPromoNonModalSigninPasswordFeature)))
      .Times(1);
  infobar->owner()->RemoveInfoBar(infobar);
  testing::Mock::VerifyAndClearExpectations(mock_tracker_);

  histogram_tester.ExpectBucketCount(kPasswordPromoHistogram,
                                     NonModalSignInPromoAction::kTimeout, 0);
  EXPECT_EQ(1, fake_delegate_.shouldDismissCount);

  // Disconnecting afterward must not notify the tracker a second time.
  EXPECT_CALL(*mock_tracker_, Dismissed(testing::_)).Times(0);
  [mediator_ disconnect];
}

// Tests that the promo is dismissed without recording a timeout when the user
// signs in.
TEST_F(NonModalSignInPromoMediatorTest, DismissWhenUserSignsIn) {
  base::HistogramTester histogram_tester;
  AllowPromo();
  StartPromo();
  InfoBarIOS* infobar = GetPromoInfobar();
  ASSERT_TRUE(infobar);
  PresentBanner(infobar);

  authentication_service_->SignIn(identity_,
                                  signin_metrics::AccessPoint::kStartPage);

  EXPECT_FALSE(GetPromoInfobar());
  histogram_tester.ExpectBucketCount(kPasswordPromoHistogram,
                                     NonModalSignInPromoAction::kTimeout, 0);
  EXPECT_EQ(1, fake_delegate_.shouldDismissCount);
  EXPECT_EQ(mediator_, fake_delegate_.lastMediator);
}

// Tests that the promo is not displayed when the feature engagement tracker
// disallows it at the WouldTriggerHelpUI stage.
TEST_F(NonModalSignInPromoMediatorTest,
       DoNotShowPromoWhenTrackerDisallowsAtWouldStage) {
  EXPECT_CALL(
      *mock_tracker_,
      WouldTriggerHelpUI(testing::Ref(
          feature_engagement::kIPHiOSPromoNonModalSigninPasswordFeature)))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(testing::_)).Times(0);

  [mediator_ startPromoDisplayTimer];

  EXPECT_EQ(1, fake_delegate_.shouldDismissCount);
  EXPECT_FALSE(GetPromoInfobar());
}

// Tests that the promo is not displayed when the feature engagement tracker
// allows at WouldTriggerHelpUI but disallows at ShouldTriggerHelpUI.
TEST_F(NonModalSignInPromoMediatorTest,
       DoNotShowPromoWhenTrackerDisallowsAtShouldStage) {
  EXPECT_CALL(
      *mock_tracker_,
      WouldTriggerHelpUI(testing::Ref(
          feature_engagement::kIPHiOSPromoNonModalSigninPasswordFeature)))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(
      *mock_tracker_,
      ShouldTriggerHelpUI(testing::Ref(
          feature_engagement::kIPHiOSPromoNonModalSigninPasswordFeature)))
      .WillRepeatedly(testing::Return(false));

  StartPromo();

  EXPECT_EQ(1, fake_delegate_.shouldDismissCount);
  EXPECT_FALSE(GetPromoInfobar());
}

// Tests that the promo is skipped without consuming an IPH impression when the
// delegate reports that it cannot be displayed.
TEST_F(NonModalSignInPromoMediatorTest, SkipsPromoWhenDelegateCannotDisplay) {
  EXPECT_CALL(*mock_tracker_, WouldTriggerHelpUI(testing::_))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(testing::_)).Times(0);
  EXPECT_CALL(*mock_tracker_, Dismissed(testing::_)).Times(0);
  fake_delegate_.canDisplayPromo = NO;

  StartPromo();

  EXPECT_FALSE(GetPromoInfobar());
  EXPECT_EQ(1, fake_delegate_.shouldDismissCount);
}

// Tests that the promo is skipped without consuming an IPH impression when
// another infobar banner is already presented.
TEST_F(NonModalSignInPromoMediatorTest,
       SkipsPromoWhenAnotherBannerIsAlreadyShowing) {
  EXPECT_CALL(*mock_tracker_, WouldTriggerHelpUI(testing::_))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(testing::_)).Times(0);
  EXPECT_CALL(*mock_tracker_, Dismissed(testing::_)).Times(0);

  InfoBarIOS* other_infobar = AddOtherInfoBar(/*skip_banner=*/false);
  PresentBanner(other_infobar);

  StartPromo();

  EXPECT_FALSE(GetPromoInfobar());
  EXPECT_EQ(1, fake_delegate_.shouldDismissCount);
}

// Tests that changing the active tab dismisses the promo without recording a
// timeout.
TEST_F(NonModalSignInPromoMediatorTest, TabChangeDismissesPromo) {
  base::HistogramTester histogram_tester;
  AllowPromo();
  StartPromo();
  InfoBarIOS* infobar = GetPromoInfobar();
  ASSERT_TRUE(infobar);
  PresentBanner(infobar);
  InfoBarManagerImpl* manager = InfoBarManagerImpl::FromWebState(
      browser_->GetWebStateList()->GetActiveWebState());

  InsertWebState();

  EXPECT_TRUE(manager->infobars().empty());
  histogram_tester.ExpectBucketCount(kPasswordPromoHistogram,
                                     NonModalSignInPromoAction::kTimeout, 0);
  EXPECT_EQ(1, fake_delegate_.shouldDismissCount);
}

// Tests that the browser view leaving the hierarchy dismisses the promo without
// recording a timeout.
TEST_F(NonModalSignInPromoMediatorTest,
       BrowserViewLeavingHierarchyDismissesPromo) {
  base::HistogramTester histogram_tester;
  AllowPromo();
  StartPromo();
  InfoBarIOS* infobar = GetPromoInfobar();
  ASSERT_TRUE(infobar);
  PresentBanner(infobar);

  visibility_notifier_->GetNotificationCallback().Run(
      BrowserViewVisibilityState::kNotInViewHierarchy,
      BrowserViewVisibilityState::kAppearing);

  EXPECT_FALSE(GetPromoInfobar());
  histogram_tester.ExpectBucketCount(kPasswordPromoHistogram,
                                     NonModalSignInPromoAction::kTimeout, 0);
  EXPECT_EQ(1, fake_delegate_.shouldDismissCount);
}

// Tests that another banner being presented dismisses the promo without
// recording a timeout.
TEST_F(NonModalSignInPromoMediatorTest, AnotherBannerShownDismissesPromo) {
  base::HistogramTester histogram_tester;
  AllowPromo();
  StartPromo();
  ASSERT_TRUE(GetPromoInfobar());

  // Add a `skip_banner` infobar so adding it doesn't trigger the InfoBarManager
  // observer, then present its banner via the OverlayPresenter directly.
  InfoBarIOS* other_infobar = AddOtherInfoBar(/*skip_banner=*/true);
  ASSERT_TRUE(GetPromoInfobar());
  PresentBanner(other_infobar);

  EXPECT_FALSE(GetPromoInfobar());
  histogram_tester.ExpectBucketCount(kPasswordPromoHistogram,
                                     NonModalSignInPromoAction::kTimeout, 0);
  EXPECT_EQ(1, fake_delegate_.shouldDismissCount);
}

// Tests that adding another banner infobar dismisses the promo, while adding a
// `skip_banner` infobar does not.
TEST_F(NonModalSignInPromoMediatorTest,
       AnotherBannerInfoBarAddedDismissesPromo) {
  base::HistogramTester histogram_tester;
  AllowPromo();
  StartPromo();
  InfoBarIOS* infobar = GetPromoInfobar();
  ASSERT_TRUE(infobar);
  PresentBanner(infobar);

  ASSERT_TRUE(AddOtherInfoBar(/*skip_banner=*/true));
  EXPECT_TRUE(GetPromoInfobar());
  EXPECT_EQ(0, fake_delegate_.shouldDismissCount);

  ASSERT_TRUE(AddOtherInfoBar(/*skip_banner=*/false));
  EXPECT_FALSE(GetPromoInfobar());
  histogram_tester.ExpectBucketCount(kPasswordPromoHistogram,
                                     NonModalSignInPromoAction::kTimeout, 0);
  EXPECT_EQ(1, fake_delegate_.shouldDismissCount);
}

// Tests that if there is no active WebState when the display delay ends, the
// mediator dismisses the promo and still notifies the tracker on `-disconnect`.
TEST_F(NonModalSignInPromoMediatorTest,
       DismissesAndNotifiesTrackerWhenNoActiveWebState) {
  EXPECT_CALL(
      *mock_tracker_,
      WouldTriggerHelpUI(testing::Ref(
          feature_engagement::kIPHiOSPromoNonModalSigninPasswordFeature)))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(
      *mock_tracker_,
      ShouldTriggerHelpUI(testing::Ref(
          feature_engagement::kIPHiOSPromoNonModalSigninPasswordFeature)))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(
      *mock_tracker_,
      Dismissed(testing::Ref(
          feature_engagement::kIPHiOSPromoNonModalSigninPasswordFeature)))
      .Times(1);

  browser_->GetWebStateList()->DetachWebStateAt(0);
  StartPromo();

  EXPECT_EQ(1, fake_delegate_.shouldDismissCount);
  [mediator_ disconnect];
}

// Tests that the promo infobar is added after the display delay, and that
// disconnecting removes it and notifies the feature engagement tracker once.
TEST_F(NonModalSignInPromoMediatorTest, DisconnectNotifiesTracker) {
  EXPECT_CALL(
      *mock_tracker_,
      WouldTriggerHelpUI(testing::Ref(
          feature_engagement::kIPHiOSPromoNonModalSigninPasswordFeature)))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(
      *mock_tracker_,
      ShouldTriggerHelpUI(testing::Ref(
          feature_engagement::kIPHiOSPromoNonModalSigninPasswordFeature)))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(
      *mock_tracker_,
      Dismissed(testing::Ref(
          feature_engagement::kIPHiOSPromoNonModalSigninPasswordFeature)))
      .Times(1);

  [mediator_ startPromoDisplayTimer];
  EXPECT_FALSE(GetPromoInfobar());
  task_environment_.FastForwardBy(base::Seconds(1));
  ASSERT_TRUE(GetPromoInfobar());

  [mediator_ disconnect];

  EXPECT_FALSE(GetPromoInfobar());
  EXPECT_EQ(0, fake_delegate_.shouldDismissCount);
}
