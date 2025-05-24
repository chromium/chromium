// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/pickle_base_types.h"

#include <array>

#include "base/pickle.h"
#include "base/time/time.h"
#include "net/base/pickle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

using ::testing::Optional;

TEST(PickleBaseTypesTest, Time) {
  base::Time nov1994;
  ASSERT_TRUE(
      base::Time::FromUTCString("Tue, 15 Nov 1994 12:45:26 GMT", &nov1994));
  static const auto kCases = std::to_array<base::Time>(
      {base::Time(), base::Time::Max(), base::Time::UnixEpoch(), nov1994});
  for (base::Time test_case : kCases) {
    SCOPED_TRACE(test_case);
    base::Pickle pickle;
    WriteToPickle(pickle, test_case);
    EXPECT_EQ(EstimatePickleSize(test_case), pickle.payload_size());
    EXPECT_THAT(ReadValueFromPickle<base::Time>(pickle), Optional(test_case));
  }
}

}  // namespace

}  // namespace net
