// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "third_party/blink/renderer/platform/scheduler/common/process_time_override_coordinator.h"

namespace blink::scheduler {
namespace {

using testing::Eq;

constexpr base::Time kStartTime = base::Time() + base::Seconds(5000);

TEST(ProcessTimeOverrideCoordinatorTest, SingleClient) {
  const base::TimeTicks start_ticks = base::TimeTicks::Now() + base::Seconds(5);

  auto expect_never_called = base::BindRepeating([]() {
    FAIL() << "Schedule tasks callback should not be called for single client";
  });

  auto time_override = ProcessTimeOverrideCoordinator::CreateOverride(
      kStartTime, start_ticks, std::move(expect_never_called));
  EXPECT_THAT(time_override->NowTicks(), Eq(start_ticks));
  EXPECT_THAT(base::TimeTicks::Now(), Eq(start_ticks));
  EXPECT_THAT(base::Time::Now(), Eq(kStartTime));

  EXPECT_THAT(time_override->TryAdvancingTime(start_ticks + base::Seconds(1)),
              Eq(start_ticks + base::Seconds(1)));

  EXPECT_THAT(time_override->NowTicks(), Eq(start_ticks + base::Seconds(1)));
  EXPECT_THAT(base::TimeTicks::Now(), Eq(start_ticks + base::Seconds(1)));
  EXPECT_THAT(base::Time::Now(), Eq(kStartTime + base::Seconds(1)));

  // A single client can always get what it wants.
  EXPECT_THAT(time_override->TryAdvancingTime(start_ticks + base::Seconds(2)),
              Eq(start_ticks + base::Seconds(2)));
  EXPECT_THAT(time_override->NowTicks(), Eq(start_ticks + base::Seconds(2)));
}

TEST(ProcessTimeOverrideCoordinatorTest, MultipleClients) {
  const base::TimeTicks start_ticks = base::TimeTicks::Now() + base::Seconds(5);
  int client1_callback_count = 0;
  int client2_callback_count = 0;

  auto client1 = ProcessTimeOverrideCoordinator::CreateOverride(
      kStartTime, start_ticks,
      base::BindLambdaForTesting(
          [&client1_callback_count] { client1_callback_count++; }));
  EXPECT_THAT(client1->NowTicks(), Eq(start_ticks));

  // The second client won't get the requested ticks / time, because the
  // overrides are already enabled.
  auto client2 = ProcessTimeOverrideCoordinator::CreateOverride(
      kStartTime + base::Seconds(1), start_ticks + base::Seconds(1),
      base::BindLambdaForTesting(
          [&client2_callback_count] { client2_callback_count++; }));
  EXPECT_THAT(client1->NowTicks(), Eq(start_ticks));
  EXPECT_THAT(client2->NowTicks(), Eq(start_ticks));
  EXPECT_THAT(base::Time::Now(), Eq(kStartTime));

  // Nothing happens when first client tries to advance time.
  EXPECT_THAT(client1->TryAdvancingTime(start_ticks + base::Seconds(1)),
              Eq(start_ticks));
  EXPECT_THAT(base::TimeTicks::Now(), Eq(start_ticks));
  EXPECT_THAT(base::Time::Now(), Eq(kStartTime));

  EXPECT_THAT(client1_callback_count, Eq(0));
  EXPECT_THAT(client2_callback_count, Eq(0));

  // The second client succeeds in advancing time to value before first client.
  EXPECT_THAT(client2->TryAdvancingTime(start_ticks + base::Milliseconds(100)),
              Eq(start_ticks + base::Milliseconds(100)));
  EXPECT_THAT(base::TimeTicks::Now(),
              Eq(start_ticks + base::Milliseconds(100)));
  EXPECT_THAT(base::Time::Now(), Eq(kStartTime + base::Milliseconds(100)));
  EXPECT_THAT(client1_callback_count, Eq(1));
  EXPECT_THAT(client2_callback_count, Eq(0));

  // Now the second client tries to advance past first client (but can't)
  EXPECT_THAT(client2->TryAdvancingTime(start_ticks + base::Milliseconds(2000)),
              Eq(start_ticks + base::Milliseconds(1000)));
  EXPECT_THAT(base::TimeTicks::Now(),
              Eq(start_ticks + base::Milliseconds(1000)));
  EXPECT_THAT(base::Time::Now(), Eq(kStartTime + base::Milliseconds(1000)));
  EXPECT_THAT(client1_callback_count, Eq(2));
  EXPECT_THAT(client2_callback_count, Eq(0));

  // When time is held up by another client, the one ahead may repeatedly try
  // to "advance" to the same value...
  EXPECT_THAT(client2->TryAdvancingTime(start_ticks + base::Milliseconds(2000)),
              Eq(start_ticks + base::Milliseconds(1000)));
  EXPECT_THAT(client1_callback_count, Eq(2));
  EXPECT_THAT(client2_callback_count, Eq(0));

  // When time is held up by another client, the one ahead may repeatedly try
  // to "advance" to the same value, or event to an earlier value.
  EXPECT_THAT(client2->TryAdvancingTime(start_ticks + base::Milliseconds(1500)),
              Eq(start_ticks + base::Milliseconds(1000)));
  EXPECT_THAT(client1_callback_count, Eq(2));
  EXPECT_THAT(client2_callback_count, Eq(0));

  // ... and now the second catches up.
  EXPECT_THAT(client1->TryAdvancingTime(start_ticks + base::Milliseconds(1500)),
              Eq(start_ticks + base::Milliseconds(1500)));
  EXPECT_THAT(base::TimeTicks::Now(),
              Eq(start_ticks + base::Milliseconds(1500)));
  EXPECT_THAT(base::Time::Now(), Eq(kStartTime + base::Milliseconds(1500)));
  EXPECT_THAT(client1_callback_count, Eq(2));
  EXPECT_THAT(client2_callback_count, Eq(1));
}

}  // namespace

}  // namespace blink::scheduler
