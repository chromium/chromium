// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/promos_manager/promos_manager_coordinator_unittest.h"

#import <Foundation/Foundation.h>

#import "base/apple/foundation_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/promos_manager/features.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/commands/credential_provider_promo_commands.h"
#import "ios/chrome/browser/ui/promos_manager/bannered_promo_view_provider.h"
#import "ios/chrome/browser/ui/promos_manager/promos_manager_coordinator+internal.h"
#import "ios/chrome/browser/ui/promos_manager/standard_promo_action_handler.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

PromosManagerCoordinatorTest::PromosManagerCoordinatorTest() {
  browser_state_ = TestChromeBrowserState::Builder().Build();
  browser_ = std::make_unique<TestBrowser>(browser_state_.get());
  view_controller_ = [[UIViewController alloc] init];
  [scoped_key_window_.Get() setRootViewController:view_controller_];
}
PromosManagerCoordinatorTest::~PromosManagerCoordinatorTest() {}

void PromosManagerCoordinatorTest::CreatePromosManagerCoordinator() {
  coordinator_ = [[PromosManagerCoordinator alloc]
          initWithBaseViewController:view_controller_
                             browser:browser_.get()
      credentialProviderPromoHandler:OCMStrictProtocolMock(@protocol(
                                         CredentialProviderPromoCommands))];
}

// Tests a provider's standardPromoDismissAction is called when a
// viewController's dismiss button is pressed.
TEST_F(PromosManagerCoordinatorTest,
       ViewControllerDismissesUsingDismissButton) {
  CreatePromosManagerCoordinator();

  id provider = OCMProtocolMock(@protocol(StandardPromoViewProvider));
  coordinator_.provider = provider;

  OCMExpect([provider standardPromoDismissAction]);

  [coordinator_ confirmationAlertDismissAction];

  [provider verify];
}

// Tests a banneredProvider's standardPromoDismissAction is called when a
// banneredViewController's dismiss button is pressed.
TEST_F(PromosManagerCoordinatorTest,
       BanneredViewControllerDismissesUsingSecondaryButton) {
  CreatePromosManagerCoordinator();

  id banneredProvider = OCMProtocolMock(@protocol(BanneredPromoViewProvider));
  coordinator_.banneredProvider = banneredProvider;

  OCMExpect([banneredProvider standardPromoDismissAction]);

  [coordinator_ confirmationAlertDismissAction];

  [banneredProvider verify];
}

// Tests standardPromoDismissSwipe is called when a viewController is
// dismissed via swipe.
TEST_F(PromosManagerCoordinatorTest, ViewControllerDismissesViaSwipe) {
  CreatePromosManagerCoordinator();

  id provider = OCMProtocolMock(@protocol(StandardPromoViewProvider));
  coordinator_.provider = provider;

  OCMExpect([provider standardPromoDismissSwipe]);

  [coordinator_ presentationControllerDidDismiss:nil];

  [provider verify];
}

// Tests standardPromoDismissSwipe is called when a banneredViewController is
// dismissed via swipe.
TEST_F(PromosManagerCoordinatorTest, BanneredViewControllerDismissesViaSwipe) {
  CreatePromosManagerCoordinator();

  id banneredProvider = OCMProtocolMock(@protocol(BanneredPromoViewProvider));
  coordinator_.banneredProvider = banneredProvider;

  OCMExpect([banneredProvider standardPromoDismissSwipe]);

  [coordinator_ presentationControllerDidDismiss:nil];

  [banneredProvider verify];
}

// TODO(crbug.com/1370763): Add unit tests for promoWasDisplayed being
// called when promo is displayed.
