// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_entry_point_mediator.h"

#import <memory>

#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/mock_promos_manager.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"

// Test fixture for SafariDataImportEntryPointMediator.
class SafariDataImportEntryPointMediatorTest : public PlatformTest {
 public:
  SafariDataImportEntryPointMediatorTest() : PlatformTest() {
    ProfileState* profile_state = [[ProfileState alloc] initWithAppState:nil];
    scene_state_ = [[SceneState alloc] initWithAppState:nil];
    scene_state_.profileState = profile_state;
    promos_manager_ = std::make_unique<MockPromosManager>();
    mediator_ = [[SafariDataImportEntryPointMediator alloc]
        initWithUIBlockerTarget:scene_state_
                  promosManager:promos_manager_.get()];
  }

 protected:
  SceneState* scene_state_;
  std::unique_ptr<MockPromosManager> promos_manager_;
  SafariDataImportEntryPointMediator* mediator_;
};

// Tests that the Safari import reminder is registered on request.
TEST_F(SafariDataImportEntryPointMediatorTest,
       TestRegisterSafariImportReminder) {
  EXPECT_CALL(*promos_manager_.get(),
              RegisterPromoForSingleDisplay(
                  promos_manager::Promo::SafariImportRemindMeLater));
  [mediator_ registerReminder];
}
