// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser//ui/content_suggestions/tips/tips_magic_stack_mediator.h"

#import <Foundation/Foundation.h>

#import "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/ui/content_suggestions/tips/tips_module_state.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using segmentation_platform::TipIdentifier;

// Tests the `TipsMagicStackMediator`.
class TipsMagicStackMediatorTest : public PlatformTest {
 public:
  void SetUp() override {
    // Create a `TipsMagicStackMediator` with an initial unknown
    // `TipIdentifier`.
    mediator_ = [[TipsMagicStackMediator alloc]
        initWithIdentifier:TipIdentifier::kUnknown
        profilePrefService:&profile_pref_service_];
  }

  void TearDown() override { mediator_ = nil; }

 protected:
  TipsMagicStackMediator* mediator_;
  sync_preferences::TestingPrefServiceSyncable profile_pref_service_;
};

// Tests that the mediator's initial state is configured correctly for an
// unknown tip.
TEST_F(TipsMagicStackMediatorTest, HasCorrectInitialStateForUnknownTip) {
  EXPECT_EQ(TipIdentifier::kUnknown, mediator_.state.identifier);
}

// Tests that the mediator reconfigures its state to reflect a new tip.
TEST_F(TipsMagicStackMediatorTest, ReconfiguresStateForNewTip) {
  [mediator_ reconfigureWithTipIdentifier:TipIdentifier::kLensShop];

  EXPECT_EQ(TipIdentifier::kLensShop, mediator_.state.identifier);
}
