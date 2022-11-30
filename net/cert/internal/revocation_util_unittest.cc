// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/pki/revocation_util.h"

#include "net/der/encode_values.h"
#include "net/der/parse_values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

constexpr int64_t kOneHour = 3600;
constexpr int64_t kOneDay = 24 * kOneHour;
constexpr int64_t kOneWeek = 7 * kOneDay;
constexpr int64_t kWindowsEpoch = -11644473600;  // 1601-01-01 00:00:00 UTC
constexpr int64_t kMinValidTime = -62167219200;  // 0000-01-01 00:00:00 UTC
constexpr int64_t kMaxValidTime = 253402300799;  // 9999-12-31 23:59:59 UTC

}  // namespace

TEST(CheckRevocationDateTest, Valid) {
  int64_t now = time(nullptr);
  int64_t this_update = now - kOneHour;
  der::GeneralizedTime encoded_this_update;
  ASSERT_TRUE(
      der::EncodePosixTimeAsGeneralizedTime(this_update, &encoded_this_update));
  EXPECT_TRUE(
      CheckRevocationDateValid(encoded_this_update, nullptr, now, kOneWeek));

  int64_t next_update = this_update + kOneWeek;
  der::GeneralizedTime encoded_next_update;
  ASSERT_TRUE(
      der::EncodePosixTimeAsGeneralizedTime(next_update, &encoded_next_update));
  EXPECT_TRUE(CheckRevocationDateValid(encoded_this_update,
                                       &encoded_next_update, now, kOneWeek));
}

TEST(CheckRevocationDateTest, ThisUpdateInTheFuture) {
  int64_t now = time(nullptr);
  int64_t this_update = now + 3600;
  der::GeneralizedTime encoded_this_update;
  ASSERT_TRUE(
      der::EncodePosixTimeAsGeneralizedTime(this_update, &encoded_this_update));
  EXPECT_FALSE(
      CheckRevocationDateValid(encoded_this_update, nullptr, now, kOneWeek));

  int64_t next_update = this_update + kOneWeek;
  der::GeneralizedTime encoded_next_update;
  ASSERT_TRUE(
      der::EncodePosixTimeAsGeneralizedTime(next_update, &encoded_next_update));
  EXPECT_FALSE(CheckRevocationDateValid(encoded_this_update,
                                        &encoded_next_update, now, kOneWeek));
}

TEST(CheckRevocationDateTest, NextUpdatePassed) {
  int64_t now = time(nullptr);
  int64_t this_update = now - (kOneDay * 6);
  der::GeneralizedTime encoded_this_update;
  ASSERT_TRUE(
      der::EncodePosixTimeAsGeneralizedTime(this_update, &encoded_this_update));
  EXPECT_TRUE(
      CheckRevocationDateValid(encoded_this_update, nullptr, now, kOneWeek));

  int64_t next_update = now - kOneHour;
  der::GeneralizedTime encoded_next_update;
  ASSERT_TRUE(
      der::EncodePosixTimeAsGeneralizedTime(next_update, &encoded_next_update));
  EXPECT_FALSE(CheckRevocationDateValid(encoded_this_update,
                                        &encoded_next_update, now, kOneWeek));
}

TEST(CheckRevocationDateTest, NextUpdateBeforeThisUpdate) {
  int64_t now = time(nullptr);
  int64_t this_update = now - kOneDay;
  der::GeneralizedTime encoded_this_update;
  ASSERT_TRUE(
      der::EncodePosixTimeAsGeneralizedTime(this_update, &encoded_this_update));
  EXPECT_TRUE(
      CheckRevocationDateValid(encoded_this_update, nullptr, now, kOneWeek));

  int64_t next_update = this_update - kOneDay;
  der::GeneralizedTime encoded_next_update;
  ASSERT_TRUE(
      der::EncodePosixTimeAsGeneralizedTime(next_update, &encoded_next_update));
  EXPECT_FALSE(CheckRevocationDateValid(encoded_this_update,
                                        &encoded_next_update, now, kOneWeek));
}

TEST(CheckRevocationDateTest, ThisUpdateOlderThanMaxAge) {
  int64_t now = time(nullptr);
  int64_t this_update = now - kOneWeek;
  der::GeneralizedTime encoded_this_update;
  ASSERT_TRUE(
      der::EncodePosixTimeAsGeneralizedTime(this_update, &encoded_this_update));
  EXPECT_TRUE(
      CheckRevocationDateValid(encoded_this_update, nullptr, now, kOneWeek));

  int64_t next_update = now + kOneHour;
  der::GeneralizedTime encoded_next_update;
  ASSERT_TRUE(
      der::EncodePosixTimeAsGeneralizedTime(next_update, &encoded_next_update));
  EXPECT_TRUE(CheckRevocationDateValid(encoded_this_update,
                                       &encoded_next_update, now, kOneWeek));

  ASSERT_TRUE(der::EncodePosixTimeAsGeneralizedTime(this_update - 1,
                                                    &encoded_this_update));
  EXPECT_FALSE(
      CheckRevocationDateValid(encoded_this_update, nullptr, now, kOneWeek));
  EXPECT_FALSE(CheckRevocationDateValid(encoded_this_update,
                                        &encoded_next_update, now, kOneWeek));
}

TEST(CheckRevocationDateTest, VerifyTimeFromBeforeWindowsEpoch) {
  int64_t verify_time = kWindowsEpoch - kOneDay;

  int64_t now = time(nullptr);
  int64_t this_update = now - kOneHour;
  der::GeneralizedTime encoded_this_update;
  ASSERT_TRUE(
      der::EncodePosixTimeAsGeneralizedTime(this_update, &encoded_this_update));
  EXPECT_FALSE(CheckRevocationDateValid(encoded_this_update, nullptr,
                                        verify_time, kOneWeek));

  int64_t next_update = this_update + kOneWeek;
  der::GeneralizedTime encoded_next_update;
  ASSERT_TRUE(
      der::EncodePosixTimeAsGeneralizedTime(next_update, &encoded_next_update));
  EXPECT_FALSE(CheckRevocationDateValid(
      encoded_this_update, &encoded_next_update, verify_time, kOneWeek));
}

TEST(CheckRevocationDateTest, VerifyTimeMinusAgeFromBeforeWindowsEpoch) {
  int64_t verify_time = kWindowsEpoch + kOneDay;

  int64_t this_update = kWindowsEpoch;
  der::GeneralizedTime encoded_this_update;
  ASSERT_TRUE(
      der::EncodePosixTimeAsGeneralizedTime(this_update, &encoded_this_update));
  // Note: Unlike base/time, BoringSSL can convert POSIX times a day before the
  // Windows epoch on all platforms.
  EXPECT_TRUE(CheckRevocationDateValid(encoded_this_update, nullptr,
                                       verify_time, kOneWeek));
}

TEST(CheckRevocationDateTest, VerifyTimeAtMinTime) {
  int64_t verify_time = kMinValidTime;

  int64_t now = time(nullptr);
  int64_t this_update = now - kOneHour;
  der::GeneralizedTime encoded_this_update;
  ASSERT_TRUE(
      der::EncodePosixTimeAsGeneralizedTime(this_update, &encoded_this_update));
  EXPECT_FALSE(CheckRevocationDateValid(encoded_this_update, nullptr,
                                        verify_time, kOneWeek));

  int64_t next_update = this_update + kOneWeek;
  der::GeneralizedTime encoded_next_update;
  ASSERT_TRUE(
      der::EncodePosixTimeAsGeneralizedTime(next_update, &encoded_next_update));
  // This will fail because kMinValidTime - kOneWeek is not a valid time, so
  // we expect this to not be able to compare to the allowed range.
  EXPECT_FALSE(CheckRevocationDateValid(
      encoded_this_update, &encoded_next_update, verify_time, kOneWeek));
  // This will fail because the validation time is not in the allowed range.
  EXPECT_FALSE(CheckRevocationDateValid(encoded_this_update,
                                        &encoded_next_update, verify_time, 0));
}

TEST(CheckRevocationDateTest, ThisUpdateAtMinTime) {
  int64_t this_update = kMinValidTime;
  der::GeneralizedTime encoded_this_update;
  ASSERT_TRUE(
      der::EncodePosixTimeAsGeneralizedTime(this_update, &encoded_this_update));
  int64_t next_update = kMinValidTime + kOneWeek;
  der::GeneralizedTime encoded_next_update;
  ASSERT_TRUE(
      der::EncodePosixTimeAsGeneralizedTime(next_update, &encoded_next_update));
  // This will fail because kMinValidTime - 1 is not a valid time, so
  // we expect these tests to not be able to compare to the allowed range.
  EXPECT_FALSE(
      CheckRevocationDateValid(encoded_this_update, nullptr, kMinValidTime, 1));
  EXPECT_FALSE(CheckRevocationDateValid(
      encoded_this_update, &encoded_next_update, kMinValidTime, 1));
  // Unlike the above kMinValidTime - 0 should still be valid, so we expect
  // these tests to work.
  EXPECT_TRUE(
      CheckRevocationDateValid(encoded_this_update, nullptr, kMinValidTime, 0));
  EXPECT_TRUE(CheckRevocationDateValid(encoded_this_update,
                                       &encoded_next_update, kMinValidTime, 0));

  int64_t verify_time = kMinValidTime + kOneDay;
  EXPECT_TRUE(CheckRevocationDateValid(encoded_this_update, nullptr,
                                       verify_time, kOneDay));
  EXPECT_TRUE(CheckRevocationDateValid(
      encoded_this_update, &encoded_next_update, verify_time, kOneDay));
}

TEST(CheckRevocationDateTest, VerifyTimeAtMaxTime) {
  int64_t this_update = time(nullptr) - kOneHour;
  der::GeneralizedTime encoded_this_update;
  ASSERT_TRUE(
      der::EncodePosixTimeAsGeneralizedTime(this_update, &encoded_this_update));
  int64_t next_update = this_update + kOneWeek;
  der::GeneralizedTime encoded_next_update;
  ASSERT_TRUE(
      der::EncodePosixTimeAsGeneralizedTime(next_update, &encoded_next_update));
  int64_t verify_time = kMaxValidTime;
  EXPECT_FALSE(CheckRevocationDateValid(encoded_this_update, nullptr,
                                        verify_time, kOneWeek));
  EXPECT_FALSE(CheckRevocationDateValid(
      encoded_this_update, &encoded_next_update, verify_time, kOneWeek));
}

TEST(CheckRevocationDateTest, NextUpdateAtMaxTime) {
  int64_t this_update = kMaxValidTime - kOneWeek;
  int64_t next_update = kMaxValidTime;
  der::GeneralizedTime encoded_this_update;
  ASSERT_TRUE(
      der::EncodePosixTimeAsGeneralizedTime(this_update, &encoded_this_update));
  der::GeneralizedTime encoded_next_update;
  ASSERT_TRUE(
      der::EncodePosixTimeAsGeneralizedTime(next_update, &encoded_next_update));
  // With no next_update, this is expected to work.
  EXPECT_TRUE(CheckRevocationDateValid(encoded_this_update, nullptr,
                                       kMaxValidTime, kOneWeek));
  // With a next_update, validation time must be strictly less than next_update,
  // so this should fail.
  EXPECT_FALSE(CheckRevocationDateValid(
      encoded_this_update, &encoded_next_update, kMaxValidTime, kOneWeek));

  int64_t verify_time = kMaxValidTime - kOneDay;
  EXPECT_TRUE(CheckRevocationDateValid(encoded_this_update, nullptr,
                                       verify_time, kOneWeek));
  EXPECT_TRUE(CheckRevocationDateValid(
      encoded_this_update, &encoded_next_update, verify_time, kOneWeek));
}

}  // namespace net
