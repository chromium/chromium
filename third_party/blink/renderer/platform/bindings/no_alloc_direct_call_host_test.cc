// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "no_alloc_direct_call_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class NoAllocDirectCallHostTest : public ::testing::Test {
 public:
  bool IsFallbackRequested() { return callback_options_.fallback; }

  void SetUp() override { callback_options_.fallback = false; }

  v8::FastApiCallbackOptions* callback_options() { return &callback_options_; }

 private:
  v8::FastApiCallbackOptions callback_options_ = {false, {0}};
};

TEST_F(NoAllocDirectCallHostTest, ActionsExecutedImmediatelyWhenAllocAllowed) {
  NoAllocDirectCallHost host;
  ASSERT_FALSE(host.IsInFastMode());
  bool change_me = false;
  host.PostDeferrableAction(WTF::Bind(
      [](bool* change_me) { *change_me = true; }, WTF::Unretained(&change_me)));
  ASSERT_TRUE(change_me);
  ASSERT_FALSE(host.HasDeferredActions());
  ASSERT_FALSE(IsFallbackRequested());
}

TEST_F(NoAllocDirectCallHostTest, ActionsDeferredWhenAllocDisallowed) {
  NoAllocDirectCallHost host;
  bool change_me = false;
  {
    NoAllocDirectCallScope scope(&host, callback_options());
    ASSERT_TRUE(host.IsInFastMode());
    host.PostDeferrableAction(
        WTF::Bind([](bool* change_me) { *change_me = true; },
                  WTF::Unretained(&change_me)));
  }
  ASSERT_FALSE(host.IsInFastMode());
  ASSERT_FALSE(change_me);
  ASSERT_TRUE(IsFallbackRequested());
  ASSERT_TRUE(host.HasDeferredActions());
}

TEST_F(NoAllocDirectCallHostTest, FlushDeferredActions) {
  NoAllocDirectCallHost host;
  bool change_me = false;
  {
    NoAllocDirectCallScope scope(&host, callback_options());
    host.PostDeferrableAction(
        WTF::Bind([](bool* change_me) { *change_me = true; },
                  WTF::Unretained(&change_me)));
  }
  ASSERT_TRUE(IsFallbackRequested());
  if (host.HasDeferredActions()) {
    host.FlushDeferredActions();
  }
  ASSERT_TRUE(change_me);
  ASSERT_FALSE(host.HasDeferredActions());
}

TEST_F(NoAllocDirectCallHostTest, NoAllocFallbackForAllocationFalse) {
  NoAllocDirectCallHost host;
  ASSERT_FALSE(host.NoAllocFallbackForAllocation());
  ASSERT_FALSE(IsFallbackRequested());
}

TEST_F(NoAllocDirectCallHostTest, NoAllocFallbackForAllocationTrue) {
  NoAllocDirectCallHost host;
  {
    NoAllocDirectCallScope scope(&host, callback_options());
    ASSERT_TRUE(host.NoAllocFallbackForAllocation());
  }
  ASSERT_TRUE(IsFallbackRequested());
}

TEST_F(NoAllocDirectCallHostTest, AllowAllocationDiscardsDeferredActions) {
  NoAllocDirectCallHost host;
  bool change_me = false;
  {
    NoAllocDirectCallScope scope(&host, callback_options());
    host.PostDeferrableAction(
        WTF::Bind([](bool* change_me) { *change_me = true; },
                  WTF::Unretained(&change_me)));
    ASSERT_TRUE(host.NoAllocFallbackForAllocation());
  }
  ASSERT_TRUE(IsFallbackRequested());
  ASSERT_FALSE(host.HasDeferredActions());
  ASSERT_FALSE(change_me);
}

}  // namespace blink
