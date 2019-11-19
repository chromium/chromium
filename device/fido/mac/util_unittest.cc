// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/util.h"

#include "base/time/time.h"
#include "base/time/time_override.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/ec_public_key.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_test_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace device {
namespace fido {
namespace mac {
namespace {

std::unique_ptr<ECPublicKey> TestKey() {
  return ECPublicKey::ParseX962Uncompressed(
      fido_parsing_utils::kEs256, test_data::kX962UncompressedPublicKey);
}

base::Time g_fake_now;

TEST(MakeAuthenticatorDataTest, TestTimestampSignatureCounter) {
  g_fake_now = base::Time::UnixEpoch();
  base::subtle::ScopedTimeClockOverrides time_clock_overrides(
      []() { return g_fake_now; }, nullptr, nullptr);
  const std::string rp_id = "example.com";
  const std::vector<uint8_t> credential_id = {1, 2, 3, 4, 5};
  auto opt_attested_cred_data =
      MakeAttestedCredentialData(credential_id, TestKey());
  ASSERT_TRUE(opt_attested_cred_data);
  // Epoch equals zero.
  auto auth_data =
      MakeAuthenticatorData(rp_id, std::move(opt_attested_cred_data));
  EXPECT_THAT(auth_data.counter(), ElementsAre(0x00, 0x00, 0x00, 0x00));
  // Time counter increments in seconds.
  g_fake_now += base::TimeDelta::FromSeconds(1);
  auth_data = MakeAuthenticatorData(rp_id, base::nullopt);
  EXPECT_THAT(auth_data.counter(), ElementsAre(0x00, 0x00, 0x00, 0x01));
  g_fake_now += base::TimeDelta::FromSeconds(1024);
  auth_data = MakeAuthenticatorData(rp_id, base::nullopt);
  EXPECT_THAT(auth_data.counter(), ElementsAre(0x00, 0x00, 0x04, 0x01));
  ASSERT_TRUE(base::Time::FromUTCExploded({2106, 1, 0, 1}, &g_fake_now));
  auth_data = MakeAuthenticatorData(rp_id, base::nullopt);
  EXPECT_THAT(auth_data.counter(), ElementsAre(0xff, 0xce, 0xdd, 0x80));
}

}  // namespace
}  // namespace mac
}  // namespace fido
}  // namespace device
