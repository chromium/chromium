// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/android/webauthn_cred_man_delegate.h"
#include <memory>

#include "base/test/mock_callback.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webauthn {

class WebAuthnCredManDelegateTest : public testing::Test {
 public:
  void SetUp() override {
    delegate_ = std::make_unique<WebAuthnCredManDelegate>(nullptr);
  }

  WebAuthnCredManDelegate* delegate() { return delegate_.get(); }

 private:
  std::unique_ptr<WebAuthnCredManDelegate> delegate_;
};

TEST_F(WebAuthnCredManDelegateTest, ShowCredManUiCallbackNotRunAfterCleanup) {
  base::MockCallback<base::RepeatingCallback<void(bool)>> closure;
  EXPECT_CALL(closure, Run(testing::_)).Times(0);
  delegate()->OnCredManConditionalRequestPending(true, closure.Get());

  EXPECT_CALL(closure, Run(false)).Times(1);
  delegate()->TriggerCredManUi(
      WebAuthnCredManDelegate::RequestPasswords(false));

  EXPECT_CALL(closure, Run(false)).Times(0);
  delegate()->CleanUpConditionalRequest();

  EXPECT_CALL(closure, Run(false)).Times(0);
  delegate()->TriggerCredManUi(
      WebAuthnCredManDelegate::RequestPasswords(false));
}

TEST_F(WebAuthnCredManDelegateTest, RequestCompletionCallbackRun) {
  base::MockCallback<base::RepeatingCallback<void(bool)>>
      mock_request_completion_callback;
  base::MockCallback<base::RepeatingCallback<void(bool)>> mock_full_request;
  delegate()->SetRequestCompletionCallback(
      mock_request_completion_callback.Get());

  EXPECT_CALL(mock_request_completion_callback, Run(false)).Times(1);
  delegate()->OnCredManUiClosed(false);

  // Cleaning up conditional request should not clean the request completion
  // callback.
  EXPECT_CALL(mock_request_completion_callback, Run(true)).Times(1);
  delegate()->CleanUpConditionalRequest();
  delegate()->OnCredManConditionalRequestPending(true, mock_full_request.Get());
  delegate()->OnCredManUiClosed(true);
}

TEST_F(WebAuthnCredManDelegateTest,
       IfNoFillingCallbackDoesNotRequestPasswords) {
  base::MockCallback<base::RepeatingCallback<void(bool)>> mock_cred_man_request;
  delegate()->OnCredManConditionalRequestPending(true,
                                                 mock_cred_man_request.Get());

  EXPECT_CALL(mock_cred_man_request, Run(false)).Times(1);
  delegate()->TriggerCredManUi(WebAuthnCredManDelegate::RequestPasswords(true));
}

}  // namespace webauthn
