// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/model/sync/utils/sync_error_infobar_delegate.h"

#import <memory>

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_mock_clock_override.h"
#import "components/infobars/core/infobar.h"
#import "components/infobars/core/infobar_manager.h"
#import "components/sync/service/sync_service_utils.h"
#import "components/sync/test/mock_sync_service.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/model/infobar_utils.h"
#import "ios/chrome/browser/settings/model/sync/utils/sync_presenter.h"
#import "ios/chrome/browser/settings/model/sync/utils/sync_util.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/sync/model/mock_sync_service_utils.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/models/image_model.h"

namespace {

using ::testing::Return;

constexpr SyncErrorInfoBarTrigger kSyncErrorInfoBarTrigger =
    SyncErrorInfoBarTrigger::kNewTabOpened;

class SyncErrorInfobarDelegateTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));
    profile_ = std::move(builder).Build();
    presenter_ = OCMStrictProtocolMock(@protocol(SyncPresenter));
    web_state_.SetBrowserState(profile_.get());
    // Navigation manager is needed for infobar manager.
    web_state_.SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    InfoBarManagerImpl::CreateForWebState(&web_state_);
  }

  void TearDown() override {
    EXPECT_OCMOCK_VERIFY((id)presenter_);
    PlatformTest::TearDown();
  }

  syncer::MockSyncService* mock_sync_service() {
    return static_cast<syncer::MockSyncService*>(
        SyncServiceFactory::GetForProfile(profile_.get()));
  }

  infobars::InfoBarManager* infobar_manager() {
    return InfoBarManagerImpl::FromWebState(&web_state_);
  }

  id<SyncPresenter> presenter_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  base::HistogramTester histogram_tester_;
  base::ScopedMockClockOverride scoped_clock_;
  web::FakeWebState web_state_;
};

TEST_F(SyncErrorInfobarDelegateTest, SyncServiceSignInNeedsUpdate) {
  ON_CALL(*mock_sync_service(), GetUserActionableError())
      .WillByDefault(
          Return(syncer::SyncService::UserActionableError::kSignInNeedsUpdate));

  OCMExpect([presenter_ showPrimaryAccountReauth]);
  auto delegate = std::make_unique<SyncErrorInfoBarDelegate>(
      profile_.get(), presenter_, kSyncErrorInfoBarTrigger);

  EXPECT_FALSE(delegate->Accept());
}

// Tests that the user sign-out while the infobar is displayed, check that
// nothing occurs when accept is called.
TEST_F(SyncErrorInfobarDelegateTest, SyncServiceSignInNeedsUpdateAndSignout) {
  ON_CALL(*mock_sync_service(), GetUserActionableError())
      .WillByDefault(
          Return(syncer::SyncService::UserActionableError::kSignInNeedsUpdate));

  auto delegate_unique_ptr = std::make_unique<SyncErrorInfoBarDelegate>(
      profile_.get(), presenter_, kSyncErrorInfoBarTrigger);

  SyncErrorInfoBarDelegate* delegate = delegate_unique_ptr.get();
  // The infobar must be set, otherwise the delegate believes the infobar is
  // being stopped.
  infobars::InfoBar infobar(std::move(delegate_unique_ptr));

  ON_CALL(*mock_sync_service(), GetUserActionableError())
      .WillByDefault(Return(syncer::SyncService::UserActionableError::kNone));
  delegate->OnStateChanged(mock_sync_service());

  // The actual test here is that `presenter_` is not called on anything. It’s a
  // strick mock, so any call on it would cause the test to fail.

  EXPECT_FALSE(delegate->Accept());
}

TEST_F(SyncErrorInfobarDelegateTest, SyncServiceUnrecoverableError) {
  OCMExpect([presenter_ showAccountSettings]);
  auto delegate = std::make_unique<SyncErrorInfoBarDelegate>(
      profile_.get(), presenter_, kSyncErrorInfoBarTrigger);

  EXPECT_FALSE(delegate->Accept());
}

TEST_F(SyncErrorInfobarDelegateTest, SyncServiceNeedsPassphrase) {
  ON_CALL(*mock_sync_service(), GetUserActionableError())
      .WillByDefault(
          Return(syncer::SyncService::UserActionableError::kNeedsPassphrase));

  OCMExpect([presenter_ showSyncPassphraseSettings]);
  auto delegate = std::make_unique<SyncErrorInfoBarDelegate>(
      profile_.get(), presenter_, kSyncErrorInfoBarTrigger);

  EXPECT_FALSE(delegate->Accept());
}

TEST_F(SyncErrorInfobarDelegateTest, SyncServiceNeedsTrustedVaultKey) {
  ON_CALL(*mock_sync_service(), GetUserActionableError())
      .WillByDefault(Return(syncer::SyncService::UserActionableError::
                                kNeedsTrustedVaultKeyForEverything));

  OCMExpect(
      [presenter_ showTrustedVaultReauthForFetchKeysWithTrigger:
                      trusted_vault ::TrustedVaultUserActionTriggerForUMA::
                          kNewTabPageInfobar]);
  auto delegate = std::make_unique<SyncErrorInfoBarDelegate>(
      profile_.get(), presenter_, kSyncErrorInfoBarTrigger);

  EXPECT_FALSE(delegate->Accept());
}

TEST_F(SyncErrorInfobarDelegateTest,
       SyncServiceTrustedVaultRecoverabilityDegraded) {
  ON_CALL(*mock_sync_service(), GetUserActionableError())
      .WillByDefault(
          Return(syncer::SyncService::UserActionableError::
                     kTrustedVaultRecoverabilityDegradedForEverything));

  OCMExpect(
      [presenter_ showTrustedVaultReauthForDegradedRecoverabilityWithTrigger:
                      trusted_vault ::TrustedVaultUserActionTriggerForUMA::
                          kNewTabPageInfobar]);
  auto delegate = std::make_unique<SyncErrorInfoBarDelegate>(
      profile_.get(), presenter_, kSyncErrorInfoBarTrigger);

  EXPECT_FALSE(delegate->Accept());
}

TEST_F(SyncErrorInfobarDelegateTest, LogsMetricOnDismissal) {
  ON_CALL(*mock_sync_service(), GetUserActionableError())
      .WillByDefault(Return(syncer::SyncService::UserActionableError::
                                kNeedsTrustedVaultKeyForPasswords));

  auto delegate = std::make_unique<SyncErrorInfoBarDelegate>(
      profile_.get(), presenter_, kSyncErrorInfoBarTrigger);

  delegate->InfoBarDismissed();
  constexpr int kSyncNeedsTrustedVaultKeyBucket = 6;
  histogram_tester_.ExpectUniqueSample("Sync.SyncErrorInfobarDismissed",
                                       kSyncNeedsTrustedVaultKeyBucket,
                                       /*count=*/1);
}

TEST_F(SyncErrorInfobarDelegateTest, InfobarNotCreatedBeforeTimeoutEnds) {
  ON_CALL(*mock_sync_service(), GetUserActionableError())
      .WillByDefault(Return(syncer::SyncService::UserActionableError::
                                kNeedsTrustedVaultKeyForPasswords));

  auto delegate = std::make_unique<SyncErrorInfoBarDelegate>(
      profile_.get(), presenter_, kSyncErrorInfoBarTrigger);

  // Trigger recording last infobar dismissal time. Advance the time close to
  // the timeout, but still before. Double check it is not displayed again.
  delegate->InfoBarDismissed();
  scoped_clock_.Advance(kSyncErrorInfobarTimeout - base::Minutes(1));
  EXPECT_FALSE(SyncErrorInfoBarDelegate::Create(
      infobar_manager(), profile_.get(), presenter_, kSyncErrorInfoBarTrigger));
}

TEST_F(SyncErrorInfobarDelegateTest, InfobarCreatedAgainAfterTimeout) {
  ON_CALL(*mock_sync_service(), GetUserActionableError())
      .WillByDefault(Return(syncer::SyncService::UserActionableError::
                                kNeedsTrustedVaultKeyForPasswords));

  auto delegate = std::make_unique<SyncErrorInfoBarDelegate>(
      profile_.get(), presenter_, kSyncErrorInfoBarTrigger);

  // Trigger recording last infobar dismissal time. Advance the time after the
  // timeout is over and confirm it is created again.
  delegate->InfoBarDismissed();
  scoped_clock_.Advance(kSyncErrorInfobarTimeout + base::Minutes(1));
  EXPECT_TRUE(SyncErrorInfoBarDelegate::Create(
      infobar_manager(), profile_.get(), presenter_, kSyncErrorInfoBarTrigger));
}

// Tests that after the infobar is ignored by the user and dismissed by timeout,
// the separate timeout kicks in to not display infobar for a defined period.
TEST_F(SyncErrorInfobarDelegateTest, InfobarTimeoutActiveAfterIgnoredByUser) {
  ON_CALL(*mock_sync_service(), GetUserActionableError())
      .WillByDefault(Return(syncer::SyncService::UserActionableError::
                                kNeedsTrustedVaultKeyForPasswords));

  auto delegate = std::make_unique<SyncErrorInfoBarDelegate>(
      profile_.get(), presenter_, kSyncErrorInfoBarTrigger);

  // Inform delegate that the infobar was dismissed through its timeout.
  delegate->InfoBarDismissedByTimeout();

  // Advance the time right before `kSyncErrorInfobarTimeout` runs out and check
  // that infobar is not created.
  scoped_clock_.Advance(kSyncErrorInfobarTimeout - base::Minutes(1));
  EXPECT_FALSE(SyncErrorInfoBarDelegate::Create(
      infobar_manager(), profile_.get(), presenter_, kSyncErrorInfoBarTrigger));

  // Advance the time past the `kSyncErrorInfobarTimeout`. Confirm that infobar
  // is created now.
  scoped_clock_.Advance(base::Minutes(2));
  EXPECT_TRUE(SyncErrorInfoBarDelegate::Create(
      infobar_manager(), profile_.get(), presenter_, kSyncErrorInfoBarTrigger));
}

}  // namespace
