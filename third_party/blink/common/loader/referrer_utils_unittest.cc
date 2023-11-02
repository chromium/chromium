// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/loader/referrer_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(DefaultNetReferrerPolicyTest, IsCorrect) {
  EXPECT_EQ(blink::ReferrerUtils::GetDefaultNetReferrerPolicy(),
            net::ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN);
}

}  // namespace blink
