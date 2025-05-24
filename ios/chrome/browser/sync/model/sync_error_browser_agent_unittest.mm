// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sync/model/sync_error_browser_agent.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/password_manager/core/browser/mock_password_form_cache.h"
#import "components/password_manager/core/browser/mock_password_manager.h"
#import "components/sync/base/features.h"
#import "components/sync/test/test_sync_service.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/passwords/model/password_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

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

}  // namespace

// TODO(crbug.com/408165259): Add tests for other scenarios and consider adding
// EG tests.
class SyncErrorBrowserAgentTest : public PlatformTest {
 public:
  SyncErrorBrowserAgentTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        SyncServiceFactory::GetInstance(),
        base::BindRepeating(
            [](web::BrowserState* context) -> std::unique_ptr<KeyedService> {
              return std::make_unique<syncer::TestSyncService>();
            }));
    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    sync_service_ = static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(profile_.get()));
    SyncErrorBrowserAgent::CreateForBrowser(browser_.get());
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  raw_ptr<syncer::TestSyncService> sync_service_;
};

TEST_F(SyncErrorBrowserAgentTest,
       InfobarDisplayedOnPasswordFormParsedWithPasswordSyncError) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      syncer::kSyncTrustedVaultInfobarImprovements);

  web::WebState* web_state = InsertWebState(browser_.get());
  InfoBarManagerImpl* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state);
  EXPECT_THAT(infobar_manager->infobars(), IsEmpty());

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
