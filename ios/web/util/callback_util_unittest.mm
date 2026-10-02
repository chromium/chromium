// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/util/callback_util.h"

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/test/test_future.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using CallbackUtilTest = PlatformTest;

// Tests that the callback returned by EnsureCallbackCalled(...) behaves
// as expected.
TEST_F(CallbackUtilTest, EnsureCallbackCalled) {
  base::test::TestFuture<int> future;
  base::OnceCallback<void(int)> callback;

  // Wrap a callback that invoke the future with -1 if not called. Check
  // that calling it with a value will invoke the future with said value.
  callback = web::EnsureCallbackCalled(future.GetCallback(), -1);
  EXPECT_FALSE(future.IsReady());

  std::move(callback).Run(17);
  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Take(), 17);

  // Wrap a callback that invoke the future with -1 if not called. Check
  // that dropping the callback without calling still invokes the future
  // but with the captured default value.
  callback = web::EnsureCallbackCalled(future.GetCallback(), -1);
  EXPECT_FALSE(future.IsReady());

  callback = base::OnceCallback<void(int)>{};
  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Take(), -1);
}

// Tests that the callback returned by EnsureBlockCalled(...) behaves
// as expected.
TEST_F(CallbackUtilTest, EnsureBlockCalled) {
  base::test::TestFuture<int> future;
  base::OnceCallback<void(int)> callback;

  // Wrap a callback that invoke the future with -1 if not called. Check
  // that calling it with a value will invoke the future with said value.
  callback =
      web::EnsureBlockCalled(base::CallbackToBlock(future.GetCallback()), -1);
  EXPECT_FALSE(future.IsReady());

  std::move(callback).Run(17);
  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Take(), 17);

  // Wrap a callback that invoke the future with -1 if not called. Check
  // that dropping the callback without calling still invokes the future
  // but with the captured default value.
  callback =
      web::EnsureBlockCalled(base::CallbackToBlock(future.GetCallback()), -1);
  EXPECT_FALSE(future.IsReady());

  callback = base::OnceCallback<void(int)>{};
  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Take(), -1);
}
