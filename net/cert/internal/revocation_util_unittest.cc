// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/revocation_util.h"

#include "base/time/time.h"
#include "build/build_config.h"
#include "net/der/encode_values.h"
#include "net/der/parse_values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

constexpr base::TimeDelta kOneWeek = base::TimeDelta::FromDays(7);

}  // namespace

TEST(CheckRevocationDateTest, Valid) {
  base::Time now = base::Time::Now();
  base::Time this_update = now - base::TimeDelta::FromHours(1);
  der::GeneralizedTime encoded_this_update;
  ASSERT_TRUE(
      der::EncodeTimeAsGeneralizedTime(this_update, &encoded_this_update));
  EXPECT_TRUE(
      CheckRevocationDateValid(encoded_this_update, nullptr, now, kOneWeek));

  base::Time next_update = this_update + base::TimeDelta::FromDays(7);
  der::GeneralizedTime encoded_next_update;
  ASSERT_TRUE(
      der::EncodeTimeAsGeneralizedTime(next_update, &encoded_next_update));
  EXPECT_TRUE(CheckRevocationDateValid(encoded_this_update,
                                       &encoded_next_update, now, kOneWeek));
}

TEST(CheckRevocationDateTest, ThisUpdateInTheFuture) {
  base::Time now = base::Time::Now();
  base::Time this_update = now + base::TimeDelta::FromHours(1);
  der::GeneralizedTime encoded_this_update;
  ASSERT_TRUE(
      der::EncodeTimeAsGeneralizedTime(this_update, &encoded_this_update));
  EXPECT_FALSE(
      CheckRevocationDateValid(encoded_this_update, nullptr, now, kOneWeek));

  base::Time next_update = this_update + base::TimeDelta::FromDays(7);
  der::GeneralizedTime encoded_next_update;
  ASSERT_TRUE(
      der::EncodeTimeAsGeneralizedTime(next_update, &encoded_next_update));
  EXPECT_FALSE(CheckRevocationDateValid(encoded_this_update,
                                        &encoded_next_update, now, kOneWeek));
}

TEST(CheckRevocationDateTest, NextUpdatePassed) {
  base::Time now = base::Time::Now();
  base::Time this_update = now - base::TimeDelta::FromDays(6);
  der::GeneralizedTime encoded_this_update;
  ASSERT_TRUE(
      der::EncodeTimeAsGeneralizedTime(this_update, &encoded_this_update));
  EXPECT_TRUE(
      CheckRevocationDateValid(encoded_this_update, nullptr, now, kOneWeek));

  base::Time next_update = now - base::TimeDelta::FromHours(1);
  der::GeneralizedTime encoded_next_update;
  ASSERT_TRUE(
      der::EncodeTimeAsGeneralizedTime(next_update, &encoded_next_update));
  EXPECT_FALSE(CheckRevocationDateValid(encoded_this_update,
                                        &encoded_next_update, now, kOneWeek));
}

TEST(CheckRevocationDateTest, NextUpdateBeforeThisUpdate) {
  base::Time now = base::Time::Now();
  base::Time this_update = now - base::TimeDelta::FromDays(1);
  der::GeneralizedTime encoded_this_update;
  ASSERT_TRUE(
      der::EncodeTimeAsGeneralizedTime(this_update, &encoded_this_update));
  EXPECT_TRUE(
      CheckRevocationDateValid(encoded_this_update, nullptr, now, kOneWeek));

  base::Time next_update = this_update - base::TimeDelta::FromDays(1);
  der::GeneralizedTime encoded_next_update;
  ASSERT_TRUE(
      der::EncodeTimeAsGeneralizedTime(next_update, &encoded_next_update));
  EXPECT_FALSE(CheckRevocationDateValid(encoded_this_update,
                                        &encoded_next_update, now, kOneWeek));
}

TEST(CheckRevocationDateTest, ThisUpdateOlderThanMaxAge) {
  base::Time now = base::Time::Now();
  base::Time this_update = now - kOneWeek;
  der::GeneralizedTime encoded_this_update;
  ASSERT_TRUE(
      der::EncodeTimeAsGeneralizedTime(this_update, &encoded_this_update));
  EXPECT_TRUE(
      CheckRevocationDateValid(encoded_this_update, nullptr, now, kOneWeek));

  base::Time next_update = now + base::TimeDelta::FromHours(1);
  der::GeneralizedTime encoded_next_update;
  ASSERT_TRUE(
      der::EncodeTimeAsGeneralizedTime(next_update, &encoded_next_update));
  EXPECT_TRUE(CheckRevocationDateValid(encoded_this_update,
                                       &encoded_next_update, now, kOneWeek));

  ASSERT_TRUE(der::EncodeTimeAsGeneralizedTime(
      this_update - base::TimeDelta::FromSeconds(1), &encoded_this_update));
  EXPECT_FALSE(
      CheckRevocationDateValid(encoded_this_update, nullptr, now, kOneWeek));
  EXPECT_FALSE(CheckRevocationDateValid(encoded_this_update,
                                        &encoded_next_update, now, kOneWeek));
}

TEST(CheckRevocationDateTest, VerifyTimeFromBeforeWindowsEpoch) {
  base::Time windows_epoch;
  base::Time verify_time = windows_epoch - base::TimeDelta::FromDays(1);

  base::Time now = base::Time::Now();
  base::Time this_update = now - base::TimeDelta::FromHours(1);
  der::GeneralizedTime encoded_this_update;
  ASSERT_TRUE(
      der::EncodeTimeAsGeneralizedTime(this_update, &encoded_this_update));
  EXPECT_FALSE(CheckRevocationDateValid(encoded_this_update, nullptr,
                                        verify_time, kOneWeek));

  base::Time next_update = this_update + kOneWeek;
  der::GeneralizedTime encoded_next_update;
  ASSERT_TRUE(
      der::EncodeTimeAsGeneralizedTime(next_update, &encoded_next_update));
  EXPECT_FALSE(CheckRevocationDateValid(
      encoded_this_update, &encoded_next_update, verify_time, kOneWeek));
}

TEST(CheckRevocationDateTest, VerifyTimeMinusAgeFromBeforeWindowsEpoch) {
  base::Time windows_epoch;
  base::Time verify_time = windows_epoch + base::TimeDelta::FromDays(1);

  base::Time this_update = windows_epoch;
  der::GeneralizedTime encoded_this_update;
  ASSERT_TRUE(
      der::EncodeTimeAsGeneralizedTime(this_update, &encoded_this_update));
#if defined(OS_WIN)
  EXPECT_FALSE(CheckRevocationDateValid(encoded_this_update, nullptr,
                                        verify_time, kOneWeek));
#else
  EXPECT_TRUE(CheckRevocationDateValid(encoded_this_update, nullptr,
                                       verify_time, kOneWeek));
#endif
}

}  // namespace net
