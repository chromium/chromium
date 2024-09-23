// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/mojom/base/time.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo_base {
namespace time_unittest {

TEST(TimeTest, Time) {
  base::Time in = base::Time::Now();
  base::Time out;

  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Time>(in, out));
  EXPECT_EQ(in, out);

  // Test corner cases.
  in = base::Time();
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Time>(in, out));
  EXPECT_EQ(in, out);

  in = base::Time::Max();
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Time>(in, out));
  EXPECT_EQ(in, out);

  in = base::Time::Min();
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Time>(in, out));
  EXPECT_EQ(in, out);
}

TEST(TimeTest, JSTime) {
  base::Time in = base::Time::Now();
  base::Time out;

  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::JSTime>(in, out));
  EXPECT_EQ(in, out);

  base::Time::UnixEpoch();
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::JSTime>(in, out));
  EXPECT_EQ(in, out);

  // Test corner cases.
  in = base::Time();
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::JSTime>(in, out));
  EXPECT_EQ(in, out);

  in = base::Time::Max();
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::JSTime>(in, out));
  EXPECT_EQ(in, out);

  in = base::Time::Min();
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::JSTime>(in, out));
  EXPECT_EQ(in, out);
}

TEST(TimeTest, TimeDelta) {
  base::TimeDelta in = base::Days(123);
  base::TimeDelta out;

  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::TimeDelta>(in, out));
  EXPECT_EQ(in, out);

  // Test corner cases.
  in = base::TimeDelta();
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::TimeDelta>(in, out));
  EXPECT_EQ(in, out);

  in = base::TimeDelta::Max();
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::TimeDelta>(in, out));
  EXPECT_EQ(in, out);

  in = base::TimeDelta::Min();
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::TimeDelta>(in, out));
  EXPECT_EQ(in, out);

  in = base::TimeDelta::FiniteMax();
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::TimeDelta>(in, out));
  EXPECT_EQ(in, out);

  in = base::TimeDelta::FiniteMin();
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::TimeDelta>(in, out));
  EXPECT_EQ(in, out);
}

TEST(TimeTest, TimeTicks) {
  base::TimeTicks in = base::TimeTicks::Now();
  base::TimeTicks out;

  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::TimeTicks>(in, out));
  EXPECT_EQ(in, out);

  // Test corner cases.
  in = base::TimeTicks();
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::TimeTicks>(in, out));
  EXPECT_EQ(in, out);

  in = base::TimeTicks::Max();
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::TimeTicks>(in, out));
  EXPECT_EQ(in, out);

  in = base::TimeTicks::Min();
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::TimeTicks>(in, out));
  EXPECT_EQ(in, out);
}

}  // namespace time_unittest
}  // namespace mojo_base
