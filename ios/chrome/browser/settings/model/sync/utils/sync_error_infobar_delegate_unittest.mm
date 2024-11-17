// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/model/sync/utils/sync_error_infobar_delegate.h"

#import <memory>

#import "components/sync/service/sync_service_utils.h"
#import "components/sync/test/mock_sync_service.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_utils.h"
#import "ios/chrome/browser/settings/model/sync/utils/sync_presenter.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/sync/model/mock_sync_service_utils.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/models/image_model.h"

using ::testing::Return;

namespace {

class SyncErrorInfobarDelegateTest : public PlatformTest {
 protected:
  void SetUp() override {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));
    profile_ = std::move(builder).Build();
  }

  syncer::MockSyncService* mock_sync_service() {
    return static_cast<syncer::MockSyncService*>(
        SyncServiceFactory::GetForProfile(profile_.get()));
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
};

TEST_F(SyncErrorInfobarDelegateTest, SyncServiceSignInNeedsUpdate) {
  ON_CALL(*mock_sync_service(), GetUserActionableError())
      .WillByDefault(
          Return(syncer::SyncService::UserActionableError::kSignInNeedsUpdate));

  id presenter = OCMStrictProtocolMock(@protocol(SyncPresenter));
  [[presenter expect] showPrimaryAccountReauth];
  std::unique_ptr<SyncErrorInfoBarDelegate> delegate(
      new SyncErrorInfoBarDelegate(profile_.get(), presenter));

  EXPECT_FALSE(delegate->Accept());
}

TEST_F(SyncErrorInfobarDelegateTest, SyncServiceUnrecoverableError) {
  id presenter = OCMStrictProtocolMock(@protocol(SyncPresenter));
  [[presenter expect] showAccountSettings];
  std::unique_ptr<SyncErrorInfoBarDelegate> delegate(
      new SyncErrorInfoBarDelegate(profile_.get(), presenter));

  EXPECT_FALSE(delegate->Accept());
}

TEST_F(SyncErrorInfobarDelegateTest, SyncServiceNeedsPassphrase) {
  ON_CALL(*mock_sync_service(), GetUserActionableError())
      .WillByDefault(
          Return(syncer::SyncService::UserActionableError::kNeedsPassphrase));

  id presenter = OCMStrictProtocolMock(@protocol(SyncPresenter));
  [[presenter expect] showSyncPassphraseSettings];
  std::unique_ptr<SyncErrorInfoBarDelegate> delegate(
      new SyncErrorInfoBarDelegate(profile_.get(), presenter));

  EXPECT_FALSE(delegate->Accept());
}

TEST_F(SyncErrorInfobarDelegateTest, SyncServiceNeedsTrustedVaultKey) {
  ON_CALL(*mock_sync_service(), GetUserActionableError())
      .WillByDefault(Return(syncer::SyncService::UserActionableError::
                                kNeedsTrustedVaultKeyForEverything));

  id presenter = OCMStrictProtocolMock(@protocol(SyncPresenter));
  [[presenter expect]
      showTrustedVaultReauthForFetchKeysWithTrigger:
          syncer::TrustedVaultUserActionTriggerForUMA::kNewTabPageInfobar];
  std::unique_ptr<SyncErrorInfoBarDelegate> delegate(
      new SyncErrorInfoBarDelegate(profile_.get(), presenter));

  EXPECT_FALSE(delegate->Accept());
}

TEST_F(SyncErrorInfobarDelegateTest,
       SyncServiceTrustedVaultRecoverabilityDegraded) {
  ON_CALL(*mock_sync_service(), GetUserActionableError())
      .WillByDefault(
          Return(syncer::SyncService::UserActionableError::
                     kTrustedVaultRecoverabilityDegradedForEverything));

  id presenter = OCMStrictProtocolMock(@protocol(SyncPresenter));
  [[presenter expect]
      showTrustedVaultReauthForDegradedRecoverabilityWithTrigger:
          syncer::TrustedVaultUserActionTriggerForUMA::kNewTabPageInfobar];
  std::unique_ptr<SyncErrorInfoBarDelegate> delegate(
      new SyncErrorInfoBarDelegate(profile_.get(), presenter));

  EXPECT_FALSE(delegate->Accept());
}

}  // namespace
