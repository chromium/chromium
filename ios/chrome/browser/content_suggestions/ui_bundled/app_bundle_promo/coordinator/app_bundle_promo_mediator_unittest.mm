// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/app_bundle_promo/coordinator/app_bundle_promo_mediator.h"

#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_view_controller_audience.h"
#import "ios/chrome/test/providers/app_store_bundle/test_app_store_bundle_service.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Test fixture for `AppBundlePromoMediator`.
class AppBundlePromoMediatorTest : public PlatformTest {
 protected:
  AppBundlePromoMediatorTest() = default;
  ~AppBundlePromoMediatorTest() override = default;

  void SetUp() override {
    PlatformTest::SetUp();
    TestAppStoreBundleService* app_store_bundle_service =
        new TestAppStoreBundleService();
    mediator_to_test_ = [[AppBundlePromoMediator alloc]
        initWithAppStoreBundleService:app_store_bundle_service];
    EXPECT_NE(mediator_to_test_.config, nil);
    delegate_mock_ = OCMProtocolMock(@protocol(AppBundlePromoMediatorDelegate));
    presentation_audience_mock_ =
        OCMProtocolMock(@protocol(ContentSuggestionsViewControllerAudience));
    mediator_to_test_.delegate = delegate_mock_;
    mediator_to_test_.presentationAudience = presentation_audience_mock_;
  }

  void TearDown() override {
    delegate_mock_ = nil;
    presentation_audience_mock_ = nil;
    [mediator_to_test_ disconnect];
    mediator_to_test_ = nil;
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  AppBundlePromoMediator* mediator_to_test_;
  id delegate_mock_;
  id presentation_audience_mock_;
};

// Tests that the mediator correctly calls the delegate to remove the module.
TEST_F(AppBundlePromoMediatorTest, TestRemoveModule) {
  ProceduralBlock completion = ^{
  };
  OCMExpect(
      [delegate_mock_ removeAppBundlePromoModuleWithCompletion:completion]);
  [mediator_to_test_ removeModuleWithCompletion:completion];
  EXPECT_OCMOCK_VERIFY(delegate_mock_);
}

// Tests that the mediator correctly calls the presentation audience when the
// promo is selected.
TEST_F(AppBundlePromoMediatorTest, TestDidSelect) {
  OCMExpect([presentation_audience_mock_ didSelectAppBundlePromo]);
  [mediator_to_test_ didSelectAppBundlePromo];
  EXPECT_OCMOCK_VERIFY(presentation_audience_mock_);
}
