// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/callback_timeout_helpers.h"

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class CallbackTimeoutHelpersTest : public testing::Test {
 protected:
  void VerifyAndClearExpectations() {
    testing::Mock::VerifyAndClearExpectations(&original_callback_);
    testing::Mock::VerifyAndClearExpectations(&timeout_handler_);
  }

  // Use MOCK_TIME so we can simulate timeout.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  testing::StrictMock<base::MockOnceCallback<void(int)>> original_callback_;
  testing::StrictMock<base::MockOnceCallback<void(bool)>> timeout_handler_;
};

TEST_F(CallbackTimeoutHelpersTest, TimeoutHandler_RunBeforeTimeout) {
  auto wrapped_callback = WrapCallbackWithTimeoutHandler(
      original_callback_.Get(), base::Seconds(1), timeout_handler_.Get());

  EXPECT_CALL(original_callback_, Run(123));
  std::move(wrapped_callback).Run(123);
  VerifyAndClearExpectations();

  // Fast forward the clock to make sure `timeout_handler_` is not fired.
  task_environment_.FastForwardBy(base::Seconds(2));
}

TEST_F(CallbackTimeoutHelpersTest, TimeoutHandler_RunAfterTimeout) {
  auto wrapped_callback = WrapCallbackWithTimeoutHandler(
      original_callback_.Get(), base::Seconds(1), timeout_handler_.Get());

  EXPECT_CALL(timeout_handler_, Run(/*called_on_destruction=*/false));
  task_environment_.FastForwardBy(base::Seconds(1));
  VerifyAndClearExpectations();

  // The original callback can still run after the timeout.
  EXPECT_CALL(original_callback_, Run(123));
  std::move(wrapped_callback).Run(123);
}

TEST_F(CallbackTimeoutHelpersTest, TimeoutHandler_Timeout) {
  auto wrapped_callback = WrapCallbackWithTimeoutHandler(
      original_callback_.Get(), base::Seconds(1), timeout_handler_.Get());

  EXPECT_CALL(timeout_handler_, Run(/*called_on_destruction=*/false));
  task_environment_.FastForwardBy(base::Seconds(1));
}

TEST_F(CallbackTimeoutHelpersTest, TimeoutHandler_Destruction) {
  auto wrapped_callback = WrapCallbackWithTimeoutHandler(
      original_callback_.Get(), base::Seconds(1), timeout_handler_.Get());

  EXPECT_CALL(timeout_handler_, Run(/*called_on_destruction=*/true));
  wrapped_callback.Reset();
}

TEST_F(CallbackTimeoutHelpersTest, DefaultInvoke_RunBeforeTimeout) {
  auto wrapped_callback = WrapCallbackWithDefaultInvokeIfTimeout(
      original_callback_.Get(), base::Seconds(1), 456);

  EXPECT_CALL(original_callback_, Run(123));
  std::move(wrapped_callback).Run(123);
  VerifyAndClearExpectations();
}

TEST_F(CallbackTimeoutHelpersTest, DefaultInvoke_RunAfterTimeout) {
  auto wrapped_callback = WrapCallbackWithDefaultInvokeIfTimeout(
      original_callback_.Get(), base::Seconds(1), 456);

  EXPECT_CALL(original_callback_, Run(456));
  task_environment_.FastForwardBy(base::Seconds(1));
  VerifyAndClearExpectations();

  // Running the original callback will be a no-op.
  std::move(wrapped_callback).Run(123);
}

TEST_F(CallbackTimeoutHelpersTest, DefaultInvoke_Timeout) {
  auto wrapped_callback = WrapCallbackWithDefaultInvokeIfTimeout(
      original_callback_.Get(), base::Seconds(1), 456);

  EXPECT_CALL(original_callback_, Run(456));
  task_environment_.FastForwardBy(base::Seconds(1));
}

TEST_F(CallbackTimeoutHelpersTest, DefaultInvoke_Destruction) {
  auto wrapped_callback = WrapCallbackWithDefaultInvokeIfTimeout(
      original_callback_.Get(), base::Seconds(1), 456);

  // Dropping the `wrapped_callback` means the original callback will timeout
  // for sure, so it'll be invoked with default arguments immediately.
  EXPECT_CALL(original_callback_, Run(456));
  wrapped_callback.Reset();
}

}  // namespace media
