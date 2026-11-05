// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/remote_actor/remote_actor_callback_util.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import <optional>

#import "base/functional/callback_helpers.h"
#import "base/test/test_future.h"
#import "ios/chrome/browser/passwords/model/remote_actor/remote_actor_constants.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

class RemoteActorCallbackUtilTest : public PlatformTest {
 protected:
  RemoteActorCallbackUtilTest() {
    mock_application_ = OCMClassMock([UIApplication class]);
    OCMStub([mock_application_ sharedApplication]).andReturn(mock_application_);
  }

  ~RemoteActorCallbackUtilTest() override {
    EXPECT_OCMOCK_VERIFY(mock_application_);
    [mock_application_ stopMocking];
  }

  void ExpectOpenURL(NSURL* expected_url, BOOL opened) {
    OCMExpect([mock_application_ openURL:expected_url
                                 options:@{}
                       completionHandler:[OCMArg checkWithBlock:^BOOL(void (
                                             ^completion_handler)(BOOL)) {
                         if (completion_handler) {
                           completion_handler(opened);
                         }
                         return YES;
                       }]]);
  }

  id mock_application_;
};

// Tests that `OpenGeminiCallbackURL()` opens `googlegemini://agent-auth-result`
// with `task_id` and `status=SUCCESS` (omitting `reason`) and forwards a `YES`
// completion outcome.
TEST_F(RemoteActorCallbackUtilTest, OpenGeminiCallbackURLSuccess) {
  ExpectOpenURL(
      [NSURL
          URLWithString:[NSString
                            stringWithFormat:@"%s?%s=task-123&%s=SUCCESS",
                                             kRemoteActorGeminiCallbackBaseURL,
                                             kRemoteActorTaskIdQueryParam,
                                             kRemoteActorStatusQueryParam]],
      /*opened=*/YES);

  base::test::TestFuture<bool> completion_future;
  OpenGeminiCallbackURL("task-123", RemoteActorCallbackStatus::kSuccess,
                        /*reason=*/std::nullopt,
                        completion_future.GetCallback());
  EXPECT_TRUE(completion_future.Get());
}

// Tests that `OpenGeminiCallbackURL()` omits `reason` when `status` is
// `kCancelled` and no `reason` is provided.
TEST_F(RemoteActorCallbackUtilTest,
       OpenGeminiCallbackURLCancelledWithoutReason) {
  ExpectOpenURL(
      [NSURL
          URLWithString:[NSString
                            stringWithFormat:@"%s?%s=task-123&%s=CANCELLED",
                                             kRemoteActorGeminiCallbackBaseURL,
                                             kRemoteActorTaskIdQueryParam,
                                             kRemoteActorStatusQueryParam]],
      /*opened=*/YES);

  base::test::TestFuture<bool> completion_future;
  OpenGeminiCallbackURL("task-123", RemoteActorCallbackStatus::kCancelled,
                        /*reason=*/std::nullopt,
                        completion_future.GetCallback());
  EXPECT_TRUE(completion_future.Get());
}

// Tests that `OpenGeminiCallbackURL()` opens `googlegemini://agent-auth-result`
// with `status=CANCELLED` and `reason=NOT_SIGNED_IN` and forwards a `YES`
// completion outcome.
TEST_F(RemoteActorCallbackUtilTest, OpenGeminiCallbackURLCancelledNotSignedIn) {
  ExpectOpenURL(
      [NSURL
          URLWithString:[NSString
                            stringWithFormat:
                                @"%s?%s=task-123&%s=CANCELLED&%s=NOT_SIGNED_IN",
                                kRemoteActorGeminiCallbackBaseURL,
                                kRemoteActorTaskIdQueryParam,
                                kRemoteActorStatusQueryParam,
                                kRemoteActorReasonQueryParam]],
      /*opened=*/YES);

  base::test::TestFuture<bool> completion_future;
  OpenGeminiCallbackURL("task-123", RemoteActorCallbackStatus::kCancelled,
                        RemoteActorCallbackReason::kNotSignedIn,
                        completion_future.GetCallback());
  EXPECT_TRUE(completion_future.Get());
}

// Tests that `OpenGeminiCallbackURL()` opens `googlegemini://agent-auth-result`
// with `status=FAILED` and `reason=PROFILE_MISMATCH` and forwards a `YES`
// completion outcome.
TEST_F(RemoteActorCallbackUtilTest,
       OpenGeminiCallbackURLFailedProfileMismatch) {
  ExpectOpenURL(
      [NSURL
          URLWithString:[NSString
                            stringWithFormat:
                                @"%s?%s=task-123&%s=FAILED&%s=PROFILE_MISMATCH",
                                kRemoteActorGeminiCallbackBaseURL,
                                kRemoteActorTaskIdQueryParam,
                                kRemoteActorStatusQueryParam,
                                kRemoteActorReasonQueryParam]],
      /*opened=*/YES);

  base::test::TestFuture<bool> completion_future;
  OpenGeminiCallbackURL("task-123", RemoteActorCallbackStatus::kFailed,
                        RemoteActorCallbackReason::kProfileMismatch,
                        completion_future.GetCallback());
  EXPECT_TRUE(completion_future.Get());
}

// Tests that `OpenGeminiCallbackURL()` includes `status=FAILED` and
// `reason=UPLOAD_FAILED` in the callback URL and forwards a `NO` completion
// outcome when Gemini cannot be opened.
TEST_F(RemoteActorCallbackUtilTest,
       OpenGeminiCallbackURLFailedUploadFailedOpenFails) {
  ExpectOpenURL(
      [NSURL URLWithString:[NSString
                               stringWithFormat:
                                   @"%s?%s=task-123&%s=FAILED&%s=UPLOAD_FAILED",
                                   kRemoteActorGeminiCallbackBaseURL,
                                   kRemoteActorTaskIdQueryParam,
                                   kRemoteActorStatusQueryParam,
                                   kRemoteActorReasonQueryParam]],
      /*opened=*/NO);

  base::test::TestFuture<bool> completion_future;
  OpenGeminiCallbackURL("task-123", RemoteActorCallbackStatus::kFailed,
                        RemoteActorCallbackReason::kUploadFailed,
                        completion_future.GetCallback());
  EXPECT_FALSE(completion_future.Get());
}

// Tests that `OpenGeminiCallbackURL()` percent-encodes special characters in
// `task_id` in the callback URL query.
TEST_F(RemoteActorCallbackUtilTest, OpenGeminiCallbackURLEscapesTaskId) {
  ExpectOpenURL(
      [NSURL
          URLWithString:[NSString
                            stringWithFormat:
                                @"%s?%s=task%%2F1%%2B2%%26a%%3Db+c&%s=SUCCESS",
                                kRemoteActorGeminiCallbackBaseURL,
                                kRemoteActorTaskIdQueryParam,
                                kRemoteActorStatusQueryParam]],
      /*opened=*/YES);

  base::test::TestFuture<bool> completion_future;
  OpenGeminiCallbackURL("task/1+2&a=b c", RemoteActorCallbackStatus::kSuccess,
                        /*reason=*/std::nullopt,
                        completion_future.GetCallback());
  EXPECT_TRUE(completion_future.Get());
}

// Tests that `OpenGeminiCallbackURL()` does not call `UIApplication` `openURL:`
// and invokes `completion` with `false` when `task_id` is empty.
TEST_F(RemoteActorCallbackUtilTest, OpenGeminiCallbackURLEmptyTaskId) {
  OCMReject([mock_application_ openURL:[OCMArg any]
                               options:[OCMArg any]
                     completionHandler:[OCMArg any]]);

  base::test::TestFuture<bool> completion_future;
  OpenGeminiCallbackURL("", RemoteActorCallbackStatus::kFailed,
                        RemoteActorCallbackReason::kUploadFailed,
                        completion_future.GetCallback());
  EXPECT_FALSE(completion_future.Get());
}

// Tests that `OpenGeminiCallbackURL()` works when invoked with
// `base::DoNothing()`.
TEST_F(RemoteActorCallbackUtilTest, OpenGeminiCallbackURLDoNothingCompletion) {
  ExpectOpenURL(
      [NSURL
          URLWithString:[NSString
                            stringWithFormat:@"%s?%s=task-123&%s=SUCCESS",
                                             kRemoteActorGeminiCallbackBaseURL,
                                             kRemoteActorTaskIdQueryParam,
                                             kRemoteActorStatusQueryParam]],
      /*opened=*/YES);

  OpenGeminiCallbackURL("task-123", RemoteActorCallbackStatus::kSuccess,
                        /*reason=*/std::nullopt, base::DoNothing());
}

}  // namespace
