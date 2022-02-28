// Copyright 2018 The Chromium Authors. All rights reserved.
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

namespace device {
namespace fido {
namespace mac {
namespace {

std::unique_ptr<PublicKey> TestKey() {
  return P256PublicKey::ParseX962Uncompressed(
      static_cast<int32_t>(CoseAlgorithmIdentifier::kEs256),
      test_data::kX962UncompressedPublicKey);
}

static base::Time g_fake_now;

TEST(MakeAuthenticatorDataTest, TestTimestampSignatureCounter) {
  ASSERT_TRUE(base::Time::FromUTCExploded({2106, 1, 0, 1}, &g_fake_now));
  base::subtle::ScopedTimeClockOverrides time_clock_overrides(
      []() { return g_fake_now; }, nullptr, nullptr);

  const std::string rp_id = "example.com";
  const std::vector<uint8_t> credential_id = {1, 2, 3, 4, 5};
  auto opt_attested_cred_data =
      MakeAttestedCredentialData(credential_id, TestKey());
  ASSERT_TRUE(opt_attested_cred_data);
  for (auto version :
       {CredentialMetadata::Version::kV0, CredentialMetadata::Version::kV1,
        CredentialMetadata::Version::kV2}) {
    auto auth_data = MakeAuthenticatorData(version, rp_id,
                                           std::move(opt_attested_cred_data));
    // The counter should be timestamp-based pre-V2, and then fixed at 0.
    if (version < CredentialMetadata::Version::kV2) {
      EXPECT_THAT(auth_data.counter(), ElementsAre(0xff, 0xce, 0xdd, 0x80));
    } else {
      EXPECT_THAT(auth_data.counter(), ElementsAre(0x00, 0x00, 0x00, 0x00));
    }
  }
}

}  // namespace
}  // namespace mac
}  // namespace fido
}  // namespace device
