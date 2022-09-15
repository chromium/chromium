// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/types.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Optional;

namespace network {
namespace internal {

// trust_tokens/types.h's TimeToString/StringToTime implementations are
// thin wrappers around well-tested //base conversion methods, so these
// tests are just sanity checks to make sure that values are actually
// getting passed to the pertinent //base libraries.

TEST(TrustTokenTypes, TimeToStringRoundtrip) {
  auto my_time = base::Time::UnixEpoch() +
                 base::Milliseconds(373849174829374);  // arbitrary
  EXPECT_THAT(StringToTime(TimeToString(my_time)), Optional(my_time));
}

TEST(TrustTokenTypes, TimeFromBadStringFails) {
  EXPECT_EQ(StringToTime("I bet this isn't a valid representation of a time."),
            absl::nullopt);
}

}  // namespace internal
}  // namespace network
