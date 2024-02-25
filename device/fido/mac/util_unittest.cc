// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/util.h"

#include "base/time/time.h"
#include "base/time/time_override.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/p256_public_key.h"
#include "device/fido/public_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace device::fido::mac {
namespace {

static base::Time g_fake_now;

constexpr char kRpId[] = "example.com";

TEST(MakeAuthenticatorDataTest, TestTimestampSignatureCounter) {
  ASSERT_TRUE(base::Time::FromUTCExploded({2106, 1, 0, 1}, &g_fake_now));
  base::subtle::ScopedTimeClockOverrides time_clock_overrides(
      []() { return g_fake_now; }, nullptr, nullptr);

  EXPECT_THAT(MakeAuthenticatorData(CredentialMetadata::SignCounter::kTimestamp,
                                    kRpId, std::nullopt, /*has_uv=*/true)
                  .counter(),
              ElementsAre(0xff, 0xce, 0xdd, 0x80));
  EXPECT_THAT(MakeAuthenticatorData(CredentialMetadata::SignCounter::kZero,
                                    kRpId, std::nullopt, /*has_uv=*/true)
                  .counter(),
              ElementsAre(0x00, 0x00, 0x00, 0x00));
}

TEST(MakeAuthenticatorDataTest, UvBit) {
  for (const bool has_uv : {false, true}) {
    AuthenticatorData data = MakeAuthenticatorData(
        CredentialMetadata::SignCounter::kZero, kRpId, std::nullopt, has_uv);
    EXPECT_TRUE(data.obtained_user_presence());
    EXPECT_EQ(data.obtained_user_verification(), has_uv);
  }
}

}  // namespace
}  // namespace device::fido::mac
