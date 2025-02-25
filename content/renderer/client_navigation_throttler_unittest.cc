// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/client_navigation_throttler.h"

#include "base/test/bind.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

TEST(ClientNavigationThrottlerTest, Empty) {
  ClientNavigationThrottler unused;
}

TEST(ClientNavigationThrottlerTest, Basic) {
  ClientNavigationThrottler throttler;
  int foo = 1;
  throttler.DispatchOrScheduleNavigation(
      base::BindLambdaForTesting([&foo]() { foo++; }));
  EXPECT_THAT(foo, testing::Eq(2));
}

TEST(ClientNavigationThrottlerTest, Throttles) {
  ClientNavigationThrottler throttler;
  int foo = 1;
  auto throttle1 = throttler.DeferNavigations();
  auto throttle2 = throttler.DeferNavigations();
  throttler.DispatchOrScheduleNavigation(
      base::BindLambdaForTesting([&foo]() { foo++; }));
  EXPECT_THAT(foo, testing::Eq(1));
  throttle1 = {};
  EXPECT_THAT(foo, testing::Eq(1));
  throttle2 = {};
  EXPECT_THAT(foo, testing::Eq(2));
}

TEST(ClientNavigationThrottlerTest, Order) {
  ClientNavigationThrottler throttler;
  int foo = 1;
  {
    auto handle = throttler.DeferNavigations();
    throttler.DispatchOrScheduleNavigation(
        base::BindLambdaForTesting([&foo]() { foo += 1; }));
    throttler.DispatchOrScheduleNavigation(
        base::BindLambdaForTesting([&foo]() { foo <<= 1; }));
    EXPECT_THAT(foo, testing::Eq(1));
  }
  EXPECT_THAT(foo, testing::Eq(4));
}

}  // namespace

}  // namespace content
