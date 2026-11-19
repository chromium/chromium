// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/coordinator/auto_deletion/auto_deletion_promo_mediator.h"

#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/download/ui/auto_deletion/auto_deletion_promo_commands.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Test fixture for `AutoDeletionPromoMediator`.
class AutoDeletionPromoMediatorTest : public PlatformTest {
 protected:
  AutoDeletionPromoMediatorTest() {
    local_state_.registry()->RegisterBooleanPref(
        prefs::kDownloadAutoDeletionEnabled, false);
    auto_deletion_promo_handler_mock_ =
        OCMStrictProtocolMock(@protocol(AutoDeletionPromoCommands));
    mediator_ =
        [[AutoDeletionPromoMediator alloc] initWithLocalState:&local_state_];
    mediator_.autoDeletionPromoHandler = auto_deletion_promo_handler_mock_;
  }

  ~AutoDeletionPromoMediatorTest() override {
    [mediator_ disconnect];
    EXPECT_OCMOCK_VERIFY(auto_deletion_promo_handler_mock_);
  }

  TestingPrefServiceSimple local_state_;
  id auto_deletion_promo_handler_mock_;
  AutoDeletionPromoMediator* mediator_;
};

// Tests that enabling Auto-deletion turns the feature on and asks the handler
// to present the enrollment action sheet.
TEST_F(AutoDeletionPromoMediatorTest, EnableAutoDeletion) {
  OCMExpect([auto_deletion_promo_handler_mock_ presentAutoDeletionActionSheet]);

  [mediator_ enableAutoDeletion];

  EXPECT_TRUE(local_state_.GetBoolean(prefs::kDownloadAutoDeletionEnabled));
}

// Tests that enabling Auto-deletion after `-disconnect` neither changes the
// pref nor notifies the handler.
TEST_F(AutoDeletionPromoMediatorTest, EnableAutoDeletionAfterDisconnect) {
  [mediator_ disconnect];

  // `auto_deletion_promo_handler_mock_` is a strict mock, so any call to it
  // fails the test.
  [mediator_ enableAutoDeletion];

  EXPECT_FALSE(local_state_.GetBoolean(prefs::kDownloadAutoDeletionEnabled));
}
