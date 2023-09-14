// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/nss_util.h"

#include <prtime.h>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crypto {

TEST(NSSUtilTest, PRTimeConversion) {
  EXPECT_EQ(base::Time::UnixEpoch(), PRTimeToBaseTime(0));
  EXPECT_EQ(0, BaseTimeToPRTime(base::Time::UnixEpoch()));

  static constexpr PRExplodedTime kPrxtime = {
      .tm_usec = 342000,
      .tm_sec = 19,
      .tm_min = 52,
      .tm_hour = 2,
      .tm_mday = 10,
      .tm_month = 11,  // 0-based
      .tm_year = 2011,
      .tm_params = {.tp_gmt_offset = 0, .tp_dst_offset = 0}};
  PRTime pr_time = PR_ImplodeTime(&kPrxtime);

  static constexpr base::Time::Exploded kExploded = {.year = 2011,
                                                     .month = 12,  // 1-based
                                                     .day_of_month = 10,
                                                     .hour = 2,
                                                     .minute = 52,
                                                     .second = 19,
                                                     .millisecond = 342};
  base::Time base_time;
  EXPECT_TRUE(base::Time::FromUTCExploded(kExploded, &base_time));

  EXPECT_EQ(base_time, PRTimeToBaseTime(pr_time));
  EXPECT_EQ(pr_time, BaseTimeToPRTime(base_time));
}

}  // namespace crypto
