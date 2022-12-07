// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/sync/utils/sync_error_infobar_delegate.h"

#import <memory>

#import "base/test/scoped_feature_list.h"
#import "components/sync/driver/sync_service_utils.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/infobar_utils.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service_mock.h"
#import "ios/chrome/browser/ui/icons/symbols.h"
#import "ios/chrome/browser/ui/settings/sync/utils/sync_presenter.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/models/image_model.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using ::testing::Return;

namespace {

class SyncErrorInfobarDelegateTest : public PlatformTest {
 protected:
  void SetUp() override {
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        SyncSetupServiceFactory::GetInstance(),
        base::BindRepeating(&SyncSetupServiceMock::CreateKeyedService));
    chrome_browser_state_ = builder.Build();
  }

  SyncSetupServiceMock* sync_setup_service_mock() {
    return static_cast<SyncSetupServiceMock*>(
        SyncSetupServiceFactory::GetForBrowserState(
            chrome_browser_state_.get()));
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
};

// Tests that the delegate's icon configurations is correct when UseSymbol is
// enabled.
TEST_F(SyncErrorInfobarDelegateTest, IconConfigsUseSymbol) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kUseSFSymbols);

  id presenter = OCMStrictProtocolMock(@protocol(SyncPresenter));
  std::unique_ptr<SyncErrorInfoBarDelegate> delegate(
      new SyncErrorInfoBarDelegate(chrome_browser_state_.get(), presenter));

  EXPECT_TRUE(delegate->UseIconBackgroundTint());
  EXPECT_NSEQ([UIColor colorNamed:kTextPrimaryColor],
              delegate -> GetIconImageTintColor());
  EXPECT_NSEQ([UIColor colorNamed:kRed500Color],
              delegate -> GetIconBackgroundColor());
  EXPECT_NSEQ(DefaultSymbolTemplateWithPointSize(kSyncErrorSymbol,
                                                 kInfobarSymbolPointSize),
              delegate->GetIcon().GetImage().ToUIImage());
}

// Tests that the delegate's icon configurations is correct when legacy image
// asset is used.
TEST_F(SyncErrorInfobarDelegateTest, IconConfigsNotUseSymbol) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kUseSFSymbols);

  id presenter = OCMStrictProtocolMock(@protocol(SyncPresenter));
  std::unique_ptr<SyncErrorInfoBarDelegate> delegate(
      new SyncErrorInfoBarDelegate(chrome_browser_state_.get(), presenter));

  EXPECT_FALSE(delegate->UseIconBackgroundTint());
  EXPECT_EQ(nullptr, delegate->GetIconImageTintColor());
  EXPECT_EQ(nullptr, delegate->GetIconBackgroundColor());
}

TEST_F(SyncErrorInfobarDelegateTest, SyncServiceSignInNeedsUpdate) {
  ON_CALL(*sync_setup_service_mock(), GetSyncServiceState())
      .WillByDefault(Return(SyncSetupService::kSyncServiceSignInNeedsUpdate));

  id presenter = OCMStrictProtocolMock(@protocol(SyncPresenter));
  [[presenter expect] showReauthenticateSignin];
  std::unique_ptr<SyncErrorInfoBarDelegate> delegate(
      new SyncErrorInfoBarDelegate(chrome_browser_state_.get(), presenter));

  EXPECT_FALSE(delegate->Accept());
}

TEST_F(SyncErrorInfobarDelegateTest, SyncServiceUnrecoverableError) {
  id presenter = OCMStrictProtocolMock(@protocol(SyncPresenter));
  [[presenter expect] showAccountSettings];
  std::unique_ptr<SyncErrorInfoBarDelegate> delegate(
      new SyncErrorInfoBarDelegate(chrome_browser_state_.get(), presenter));

  EXPECT_FALSE(delegate->Accept());
}

TEST_F(SyncErrorInfobarDelegateTest, SyncServiceNeedsPassphrase) {
  ON_CALL(*sync_setup_service_mock(), GetSyncServiceState())
      .WillByDefault(Return(SyncSetupService::kSyncServiceNeedsPassphrase));

  id presenter = OCMStrictProtocolMock(@protocol(SyncPresenter));
  [[presenter expect] showSyncPassphraseSettings];
  std::unique_ptr<SyncErrorInfoBarDelegate> delegate(
      new SyncErrorInfoBarDelegate(chrome_browser_state_.get(), presenter));

  EXPECT_FALSE(delegate->Accept());
}

TEST_F(SyncErrorInfobarDelegateTest, SyncServiceNeedsTrustedVaultKey) {
  ON_CALL(*sync_setup_service_mock(), GetSyncServiceState())
      .WillByDefault(
          Return(SyncSetupService::kSyncServiceNeedsTrustedVaultKey));
  ON_CALL(*sync_setup_service_mock(), IsEncryptEverythingEnabled())
      .WillByDefault(Return(true));

  id presenter = OCMStrictProtocolMock(@protocol(SyncPresenter));
  [[presenter expect]
      showTrustedVaultReauthForFetchKeysWithTrigger:
          syncer::TrustedVaultUserActionTriggerForUMA::kNewTabPageInfobar];
  std::unique_ptr<SyncErrorInfoBarDelegate> delegate(
      new SyncErrorInfoBarDelegate(chrome_browser_state_.get(), presenter));

  EXPECT_FALSE(delegate->Accept());
}

TEST_F(SyncErrorInfobarDelegateTest,
       SyncServiceTrustedVaultRecoverabilityDegraded) {
  ON_CALL(*sync_setup_service_mock(), GetSyncServiceState())
      .WillByDefault(Return(
          SyncSetupService::kSyncServiceTrustedVaultRecoverabilityDegraded));
  ON_CALL(*sync_setup_service_mock(), IsEncryptEverythingEnabled())
      .WillByDefault(Return(true));

  id presenter = OCMStrictProtocolMock(@protocol(SyncPresenter));
  [[presenter expect]
      showTrustedVaultReauthForDegradedRecoverabilityWithTrigger:
          syncer::TrustedVaultUserActionTriggerForUMA::kNewTabPageInfobar];
  std::unique_ptr<SyncErrorInfoBarDelegate> delegate(
      new SyncErrorInfoBarDelegate(chrome_browser_state_.get(), presenter));

  EXPECT_FALSE(delegate->Accept());
}

}  // namespace
