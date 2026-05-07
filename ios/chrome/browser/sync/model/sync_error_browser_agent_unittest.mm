// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sync/model/sync_error_browser_agent.h"

#import "base/test/metrics/histogram_tester.h"
#import "components/password_manager/core/browser/mock_password_form_cache.h"
#import "components/password_manager/core/browser/mock_password_manager.h"
#import "components/sync/test/test_sync_service.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_test_utils.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_presenter.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/passwords/model/password_tab_helper.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/sync_presenter_commands.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

using ::testing::IsEmpty;
using ::testing::SizeIs;

// Creates test web state and inserts it activated into the `browser`.
web::WebState* InsertWebState(Browser* browser) {
  web::WebState::CreateParams params(browser->GetProfile());
  std::unique_ptr<web::WebState> passed_web_state =
      web::WebState::Create(params);
  web::WebState* web_state = passed_web_state.get();
  InfoBarManagerImpl::CreateForWebState(web_state);
  PasswordTabHelper::CreateForWebState(web_state);
  browser->GetWebStateList()->InsertWebState(
      std::move(passed_web_state),
      WebStateList::InsertionParams::Automatic().Activate());
  return web_state;
}

class SyncErrorBrowserAgentTest : public PlatformTest {
 public:
  SyncErrorBrowserAgentTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        SyncServiceFactory::GetInstance(),
        base::BindRepeating(
            [](ProfileIOS* profile) -> std::unique_ptr<KeyedService> {
              return std::make_unique<syncer::TestSyncService>();
            }));
    profile_ = std::move(builder).Build();

    profile_state_ = [[ProfileState alloc] initWithAppState:nil];
    SetProfileStateInitStage(profile_state_, ProfileInitStage::kFinal);
    scene_state_ = [[SceneState alloc] initWithAppState:nil];
    scene_state_.profileState = profile_state_;

    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);
    sync_service_ = static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(profile_.get()));
    SyncErrorBrowserAgent::CreateForBrowser(browser_.get());

    mock_sync_presenter_ =
        OCMStrictProtocolMock(@protocol(SyncPresenterCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_sync_presenter_
                     forProtocol:@protocol(SyncPresenterCommands)];
  }

  void TearDown() override {
    EXPECT_OCMOCK_VERIFY(mock_sync_presenter_);
    PlatformTest::TearDown();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  ProfileState* profile_state_;
  SceneState* scene_state_;
  std::unique_ptr<Browser> browser_;
  raw_ptr<syncer::TestSyncService> sync_service_;
  id mock_sync_presenter_;
};

TEST_F(SyncErrorBrowserAgentTest,
       InfobarDisplayedOnPasswordFormParsedWithPasswordSyncError) {
  base::HistogramTester histogram_tester;

  web::WebState* web_state = InsertWebState(browser_.get());
  InfoBarManagerImpl* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state);
  ASSERT_THAT(infobar_manager->infobars(), IsEmpty());

  // Set a password sync error and check that the infobar is displayed on
  // password form parsed.
  sync_service_->SetPassphraseRequired();
  static_cast<password_manager::PasswordFormManagerObserver*>(
      SyncErrorBrowserAgent::FromBrowser(browser_.get()))
      ->OnPasswordFormParsed(/*form_manager=*/nullptr);
  EXPECT_THAT(infobar_manager->infobars(), SizeIs(1));

  constexpr int kSyncNeedsPassphraseBucket = 3;
  histogram_tester.ExpectUniqueSample("Sync.SyncErrorInfobarDisplayed2",
                                      kSyncNeedsPassphraseBucket, /*count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Sync.SyncErrorInfobarDisplayed2.PasswordForm",
      kSyncNeedsPassphraseBucket,
      /*count=*/1);
}

TEST_F(SyncErrorBrowserAgentTest,
       InfobarNotDisplayedOnPasswordFormParsedWithoutPasswordSyncError) {
  base::HistogramTester histogram_tester;

  web::WebState* web_state = InsertWebState(browser_.get());
  InfoBarManagerImpl* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state);
  ASSERT_THAT(infobar_manager->infobars(), IsEmpty());

  // Do not set a password sync error and check that the infobar is not
  // displayed on password form parsed.
  static_cast<password_manager::PasswordFormManagerObserver*>(
      SyncErrorBrowserAgent::FromBrowser(browser_.get()))
      ->OnPasswordFormParsed(/*form_manager=*/nullptr);
  EXPECT_THAT(infobar_manager->infobars(), IsEmpty());

  histogram_tester.ExpectTotalCount("Sync.SyncErrorInfobarDisplayed2", 0);
}

TEST_F(SyncErrorBrowserAgentTest,
       InfobarDisplayedOnWebStateInsertedWithPasswordSyncError) {
  base::HistogramTester histogram_tester;

  id mock_resignin_presenter = OCMProtocolMock(@protocol(ReSigninPresenter));

  SyncErrorBrowserAgent::FromBrowser(browser_.get())
      ->SetUIProviders(mock_resignin_presenter);

  // Set a passphrase required and check that the infobar is displayed.
  sync_service_->SetPassphraseRequired();
  web::WebState* web_state = InsertWebState(browser_.get());
  EXPECT_THAT(InfoBarManagerImpl::FromWebState(web_state)->infobars(),
              SizeIs(1));

  constexpr int kSyncNeedsPassphraseBucket = 3;
  histogram_tester.ExpectUniqueSample("Sync.SyncErrorInfobarDisplayed2",
                                      kSyncNeedsPassphraseBucket, /*count=*/1);
  histogram_tester.ExpectUniqueSample("Sync.SyncErrorInfobarDisplayed2.NewTab",
                                      kSyncNeedsPassphraseBucket,
                                      /*count=*/1);
}

TEST_F(SyncErrorBrowserAgentTest,
       InfobarNotDisplayedOnWebStateInsertedWithoutPasswordSyncError) {
  base::HistogramTester histogram_tester;

  SyncErrorBrowserAgent::FromBrowser(browser_.get())
      ->SetUIProviders(OCMProtocolMock(@protocol(ReSigninPresenter)));
  web::WebState* web_state = InsertWebState(browser_.get());
  EXPECT_THAT(InfoBarManagerImpl::FromWebState(web_state)->infobars(),
              IsEmpty());

  histogram_tester.ExpectTotalCount("Sync.SyncErrorInfobarDisplayed2", 0);
}

TEST_F(SyncErrorBrowserAgentTest,
       InfobarDisplayedOnSetUIProvidersWithPasswordSyncError) {
  base::HistogramTester histogram_tester;

  // Set sync error.
  sync_service_->SetPassphraseRequired();

  // Insert a web state. It shouldn't show the infobar yet because UI providers
  // aren't set.
  web::WebState* web_state = InsertWebState(browser_.get());
  InfoBarManagerImpl* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state);
  ASSERT_THAT(infobar_manager->infobars(), IsEmpty());

  // Now set UI providers and check that the infobar is displayed.
  id mock_resignin_presenter = OCMProtocolMock(@protocol(ReSigninPresenter));
  SyncErrorBrowserAgent::FromBrowser(browser_.get())
      ->SetUIProviders(mock_resignin_presenter);
  EXPECT_THAT(infobar_manager->infobars(), SizeIs(1));

  constexpr int kSyncNeedsPassphraseBucket = 3;
  histogram_tester.ExpectUniqueSample("Sync.SyncErrorInfobarDisplayed2",
                                      kSyncNeedsPassphraseBucket, /*count=*/1);
  histogram_tester.ExpectUniqueSample("Sync.SyncErrorInfobarDisplayed2.NewTab",
                                      kSyncNeedsPassphraseBucket,
                                      /*count=*/1);
}

TEST_F(SyncErrorBrowserAgentTest,
       InfobarDisplayedOnPasswordFormParsedWithTrustedVaultKeyRequired) {
  base::HistogramTester histogram_tester;

  web::WebState* web_state = InsertWebState(browser_.get());
  InfoBarManagerImpl* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state);
  ASSERT_THAT(infobar_manager->infobars(), IsEmpty());

  // Set trusted vault key required.
  sync_service_->SetTrustedVaultKeyRequired(true);

  static_cast<password_manager::PasswordFormManagerObserver*>(
      SyncErrorBrowserAgent::FromBrowser(browser_.get()))
      ->OnPasswordFormParsed(/*form_manager=*/nullptr);
  EXPECT_THAT(infobar_manager->infobars(), SizeIs(1));

  constexpr int kSyncNeedsTrustedVaultKeyBucket = 6;
  histogram_tester.ExpectUniqueSample("Sync.SyncErrorInfobarDisplayed2",
                                      kSyncNeedsTrustedVaultKeyBucket,
                                      /*count=*/1);
}

}  // namespace
