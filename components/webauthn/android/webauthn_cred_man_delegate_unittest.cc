// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/android/webauthn_cred_man_delegate.h"
#include <memory>

#include "base/test/mock_callback.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class WebAuthnCredManDelegateTest : public testing::Test {
 public:
  void SetUp() override {
    delegate_ = std::make_unique<WebAuthnCredManDelegate>(nullptr);
  }

  WebAuthnCredManDelegate* delegate() { return delegate_.get(); }

 private:
  std::unique_ptr<WebAuthnCredManDelegate> delegate_;
};

TEST_F(WebAuthnCredManDelegateTest, CallbackNotRunAfterCleanup) {
  base::MockRepeatingClosure closure;
  EXPECT_CALL(closure, Run()).Times(0);
  delegate()->OnCredManConditionalRequestPending(nullptr, true, closure.Get());

  EXPECT_CALL(closure, Run()).Times(1);
  delegate()->TriggerFullRequest();

  EXPECT_CALL(closure, Run()).Times(0);
  delegate()->CleanUpConditionalRequest();

  EXPECT_CALL(closure, Run()).Times(0);
  delegate()->TriggerFullRequest();
}
