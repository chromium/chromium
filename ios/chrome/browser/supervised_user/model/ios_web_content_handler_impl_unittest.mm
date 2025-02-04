// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/model/ios_web_content_handler_impl.h"

#import <memory>

#import "base/functional/callback_helpers.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/prefs/testing_pref_service.h"
#import "components/supervised_user/core/browser/supervised_user_utils.h"
#import "components/supervised_user/core/common/features.h"
#import "components/supervised_user/core/common/supervised_user_constants.h"
#import "components/supervised_user/test_support/supervised_user_url_filter_test_utils.h"
#import "ios/chrome/browser/shared/public/commands/parent_access_commands.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Test fixture for testing IOSWebContentHandlerImpl.
class IOSWebContentHandlerImplTest : public PlatformTest {
 protected:
  IOSWebContentHandlerImplTest() {
    feature_list_.InitWithFeatureState(supervised_user::kLocalWebApprovals,
                                       YES);
  }

  void SetUp() override {
    PlatformTest::SetUp();
    mock_parent_access_commands_handler_ =
        OCMStrictProtocolMock(@protocol(ParentAccessCommands));
    web_content_handler_ = std::make_unique<IOSWebContentHandlerImpl>(
        &web_state_, mock_parent_access_commands_handler_,
        /*is_main_frame=*/true);
    url_formatter_ = std::make_unique<supervised_user::UrlFormatter>(
        filter_, supervised_user::FilteringBehaviorReason::DEFAULT);
  }

  IOSWebContentHandlerImpl* web_content_handler() {
    return web_content_handler_.get();
  }

  supervised_user::UrlFormatter* url_formatter() {
    return url_formatter_.get();
  }

  base::HistogramTester histogram_tester_;
  id mock_parent_access_commands_handler_;
  web::FakeWebState web_state_;

 private:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<supervised_user::UrlFormatter> url_formatter_;
  TestingPrefServiceSimple pref_service_;
  base::test::ScopedFeatureList feature_list_;

  supervised_user::SupervisedUserURLFilter filter_ =
      supervised_user::SupervisedUserURLFilter(
          pref_service_,
          std::make_unique<supervised_user::FakeURLFilterDelegate>());

  std::unique_ptr<IOSWebContentHandlerImpl> web_content_handler_;
};

TEST_F(IOSWebContentHandlerImplTest, HideParentAccessBottomsheetNotOnScreen) {
  OCMExpect([mock_parent_access_commands_handler_ hideParentAccessBottomSheet]);

  web_content_handler()->MaybeCloseLocalApproval();
  histogram_tester_.ExpectTotalCount(
      supervised_user::WebContentHandler::GetLocalApprovalResultHistogram(), 0);

  EXPECT_OCMOCK_VERIFY(mock_parent_access_commands_handler_);
}

TEST_F(IOSWebContentHandlerImplTest, HideParentAccessBottomsheet) {
  OCMExpect([[mock_parent_access_commands_handler_ ignoringNonObjectArgs]
      showParentAccessBottomSheetForWebState:&web_state_
                                   targetURL:GURL()
                     filteringBehaviorReason:
                         supervised_user::FilteringBehaviorReason::DEFAULT
                                  completion:[OCMArg any]]);
  OCMExpect([mock_parent_access_commands_handler_ hideParentAccessBottomSheet]);

  web_content_handler()->RequestLocalApproval(
      GURL("https://www.example.com"), u"Bob", *url_formatter(),
      supervised_user::FilteringBehaviorReason::DEFAULT, base::DoNothing());

  web_content_handler()->MaybeCloseLocalApproval();
  histogram_tester_.ExpectTotalCount(
      supervised_user::WebContentHandler::GetLocalApprovalResultHistogram(), 1);
  histogram_tester_.ExpectBucketCount(
      supervised_user::WebContentHandler::GetLocalApprovalResultHistogram(),
      supervised_user::LocalApprovalResult::kCanceled, 1);

  EXPECT_OCMOCK_VERIFY(mock_parent_access_commands_handler_);
}
