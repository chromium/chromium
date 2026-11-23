// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/signin/non_modal_promo/coordinator/non_modal_signin_promo_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "components/infobars/core/confirm_infobar_delegate.h"
#import "ios/chrome/browser/authentication/signin/non_modal_promo/coordinator/non_modal_signin_promo_types.h"
#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/model/test/fake_overlay_presentation_context.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Fake delegate counting the requests to dismiss the promo.
@interface FakeNonModalSignInPromoCoordinatorDelegate
    : NSObject <NonModalSignInPromoCoordinatorDelegate>
@property(nonatomic, assign) NSInteger dismissCount;
@end

@implementation FakeNonModalSignInPromoCoordinatorDelegate

- (void)dismissNonModalSignInPromo:
    (NonModalSignInPromoCoordinator*)coordinator {
  _dismissCount++;
}

@end

namespace {

class NonModalSignInPromoCoordinatorTest : public PlatformTest {
 protected:
  NonModalSignInPromoCoordinatorTest() {
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
    mock_tracker_ = static_cast<feature_engagement::test::MockTracker*>(
        feature_engagement::TrackerFactory::GetForProfile(profile_));

    browser_ = std::make_unique<TestBrowser>(profile_.get(),
                                             [[SceneState alloc] init]);
    OverlayPresenter::FromBrowser(browser_.get(),
                                  OverlayModality::kInfobarBanner)
        ->SetPresentationContext(&overlay_presentation_context_);
    InsertWebState();

    base_view_controller_ = [[UIViewController alloc] init];
    scoped_key_window_.Get().rootViewController = base_view_controller_;

    delegate_ = [[FakeNonModalSignInPromoCoordinatorDelegate alloc] init];
    coordinator_ = [[NonModalSignInPromoCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()
                         promoType:NonModalSignInPromoType::kBookmark];
    coordinator_.delegate = delegate_;
  }

  ~NonModalSignInPromoCoordinatorTest() override {
    [coordinator_ stop];
    [signin_coordinator_mock_ stopMocking];
    OverlayPresenter::FromBrowser(browser_.get(),
                                  OverlayModality::kInfobarBanner)
        ->SetPresentationContext(nullptr);
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

  // Makes the FET tracker allow the bookmark promo.
  void AllowPromo() {
    EXPECT_CALL(
        *mock_tracker_,
        WouldTriggerHelpUI(testing::Ref(
            feature_engagement::kIPHiOSPromoNonModalSigninBookmarkFeature)))
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(
        *mock_tracker_,
        ShouldTriggerHelpUI(testing::Ref(
            feature_engagement::kIPHiOSPromoNonModalSigninBookmarkFeature)))
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(*mock_tracker_, Dismissed(testing::_))
        .Times(testing::AnyNumber());
  }

  // Starts the coordinator and waits for the promo display delay.
  void StartPromo() {
    [coordinator_ start];
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

  // Stubs `SigninCoordinator` creation for the bookmark promo, expects
  // `-start` and `-stop`, and writes its completion block to `completion_out`.
  id StubSigninCoordinator(
      SigninCoordinatorCompletionCallback __strong* completion_out) {
    signin_coordinator_mock_ = OCMClassMock([SigninCoordinator class]);
    SigninCoordinator* signin_coordinator = signin_coordinator_mock_;
    SigninContextStyle context_style = SigninContextStyle::kDefault;
    signin_metrics::AccessPoint access_point =
        signin_metrics::AccessPoint::kNonModalSigninBookmarkPromo;
    signin_metrics::PromoAction promo_action =
        signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO;
    ChangeProfileContinuationProvider continuation_provider =
        DoNothingContinuationProvider();
    OCMStub(
        [signin_coordinator_mock_
            signinAndHistorySyncCoordinatorWithBaseViewController:
                base_view_controller_
                                                          browser:browser_.get()
                                                     contextStyle:context_style
                                                      accessPoint:access_point
                                                      promoAction:promo_action
                                              optionalHistorySync:YES
                                                  fullscreenPromo:NO
                                             continuationProvider:
                                                 continuation_provider])
        .ignoringNonObjectArgs()
        .andReturn(signin_coordinator_mock_);

    OCMExpect([signin_coordinator
        setSigninCompletion:[OCMArg checkWithBlock:^BOOL(
                                        SigninCoordinatorCompletionCallback
                                            callback) {
          *completion_out = callback;
          return callback != nil;
        }]]);
    OCMExpect([signin_coordinator start]);
    OCMExpect([signin_coordinator stop]);
    return signin_coordinator_mock_;
  }

  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::TimeSource::MOCK_TIME};
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_ = nullptr;
  raw_ptr<feature_engagement::test::MockTracker> mock_tracker_ = nullptr;
  std::unique_ptr<TestBrowser> browser_;
  FakeOverlayPresentationContext overlay_presentation_context_;
  ScopedKeyWindow scoped_key_window_;
  UIViewController* base_view_controller_ = nil;
  FakeNonModalSignInPromoCoordinatorDelegate* delegate_ = nil;
  NonModalSignInPromoCoordinator* coordinator_ = nil;
  id signin_coordinator_mock_ = nil;
};

}  // namespace

// Tests that the promo isn't counted as shown when the browser view isn't
// visible, e.g. in the tab grid.
TEST_F(NonModalSignInPromoCoordinatorTest, SkipsPromoWhenBrowserViewIsHidden) {
  EXPECT_CALL(*mock_tracker_, WouldTriggerHelpUI(testing::_))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(testing::_)).Times(0);
  EXPECT_CALL(*mock_tracker_, Dismissed(testing::_)).Times(0);
  scoped_key_window_.Get().rootViewController = nil;

  StartPromo();

  EXPECT_FALSE(GetPromoInfobar());
  EXPECT_EQ(1, delegate_.dismissCount);
}

// Tests that accepting the promo starts sign-in, keeps the coordinator alive
// while sign-in is running even after the infobar is removed, and ends the
// promo once sign-in finishes.
TEST_F(NonModalSignInPromoCoordinatorTest,
       AcceptStartsSigninAndEndsAfterCompletion) {
  AllowPromo();
  StartPromo();
  InfoBarIOS* infobar = GetPromoInfobar();
  ASSERT_TRUE(infobar);

  SigninCoordinatorCompletionCallback signin_completion = nil;
  id signin_coordinator_mock = StubSigninCoordinator(&signin_completion);

  EXPECT_TRUE(infobar->delegate()->AsConfirmInfoBarDelegate()->Accept());
  ASSERT_TRUE(signin_completion);
  EXPECT_EQ(0, delegate_.dismissCount);

  // Removing the infobar while sign-in is still running must not end the promo.
  infobar->owner()->RemoveInfoBar(infobar);
  EXPECT_EQ(0, delegate_.dismissCount);

  // Finishing sign-in now ends the promo.
  signin_completion(signin_coordinator_mock,
                    base::unexpected(SigninCoordinatorResultCanceledByUser));
  EXPECT_EQ(1, delegate_.dismissCount);

  EXPECT_OCMOCK_VERIFY(signin_coordinator_mock);
}

// Tests that if sign-in finishes before the promo infobar is removed, the
// coordinator waits for the infobar to be removed before ending the promo.
TEST_F(NonModalSignInPromoCoordinatorTest,
       AcceptSigninFinishesBeforeInfobarRemoval) {
  AllowPromo();
  StartPromo();
  InfoBarIOS* infobar = GetPromoInfobar();
  ASSERT_TRUE(infobar);

  SigninCoordinatorCompletionCallback signin_completion = nil;
  id signin_coordinator_mock = StubSigninCoordinator(&signin_completion);

  EXPECT_TRUE(infobar->delegate()->AsConfirmInfoBarDelegate()->Accept());
  ASSERT_TRUE(signin_completion);

  // Finishing sign-in while the infobar is still present must not end the
  // promo yet.
  signin_completion(signin_coordinator_mock,
                    base::unexpected(SigninCoordinatorResultCanceledByUser));
  EXPECT_EQ(0, delegate_.dismissCount);

  // Removing the infobar now ends the promo.
  infobar->owner()->RemoveInfoBar(infobar);
  EXPECT_EQ(1, delegate_.dismissCount);

  EXPECT_OCMOCK_VERIFY(signin_coordinator_mock);
}
