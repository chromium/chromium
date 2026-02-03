// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/performance_helper.h"

#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink::scheduler {

class PerformanceHelperTest : public testing::Test {
 protected:
  void SetUp() override {
    PerformanceHelper::Params params;
    params.input_boost = base::Milliseconds(100);
    params.loading_boost = base::Milliseconds(1000);
    params.scrolling_boost = base::Milliseconds(500);
    params.callback = mock_callback_.Get();

    helper_.Configure(std::move(params));
  }

  base::TimeTicks T(int ms) {
    return base::TimeTicks() + base::Milliseconds(ms);
  }

  base::MockCallback<PerformanceHelper::UpdateStateCallback> mock_callback_;
  PerformanceHelper helper_;
};

TEST_F(PerformanceHelperTest, AddAndExpires) {
  {
    testing::InSequence s;
    // First time we call, without adding anything, we should be efficient.
    EXPECT_CALL(mock_callback_, Run(true)).Times(1);
    // Multiple checks during the boosted period, should be de-bounced.
    EXPECT_CALL(mock_callback_, Run(false)).Times(1);
    // After Check at time T(1500), should be true again.
    EXPECT_CALL(mock_callback_, Run(true)).Times(1);
    // Upon destruction of the helper_ object, should get one call.
    EXPECT_CALL(mock_callback_, Run(false)).Times(1);
  }

  // First time we call, without adding anything, we should be efficient.
  helper_.Check(T(0));  // Defaults to the current time.

  // Add a long boost.
  helper_.Add(PerformanceHelper::BoostType::kPageLoad, T(1));
  helper_.Check(T(2));  // Callback should be called with false.
  helper_.Check(T(3));  // Should be de-bounced.

  // Add a secondary boost, deadline should not be extended.
  helper_.Add(PerformanceHelper::BoostType::kScroll, T(100));
  helper_.Check(T(700));  // Should be de-bounced.

  // Add another boost, deadline should be extended.
  helper_.Add(PerformanceHelper::BoostType::kTapOrTyping, T(1000));
  helper_.Check(T(1400));  // Should be de-bounced.

  // Check the expiry of that boost.
  helper_.Check(T(1500));  // Back to efficiency.
}

}  // namespace blink::scheduler
